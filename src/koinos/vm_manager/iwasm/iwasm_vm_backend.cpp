#include <wasm_c_api.h>
#include <wasm_export.h>

#include <koinos/exception.hpp>
#include <koinos/vm_manager/iwasm/exceptions.hpp>
#include <koinos/vm_manager/iwasm/iwasm_vm_backend.hpp>

#include <exception>
#include <string>

namespace koinos::vm_manager::iwasm {

namespace constants {
constexpr uint32_t iwasm_max_call_depth = 251;
constexpr std::size_t module_cache_size = 32;
} // namespace constants

iwasm_vm_backend::iwasm_vm_backend():
    _cache( constants::module_cache_size )
{}

iwasm_vm_backend::~iwasm_vm_backend() {}

std::string iwasm_vm_backend::backend_name()
{
  return "iwasm";
}

void iwasm_vm_backend::initialize()
{
  wasm_runtime_init();
}

class iwasm_runner
{
public:
  iwasm_runner( abstract_host_api& h, module_cache& c ):
      _hapi( h ),
      _cache( c )
  {}

  ~iwasm_runner();

  void register_natives();
  void load_module( const std::string& bytecode, const std::string& id );
  void instantiate_module();
  void call_start();

  static uint32_t invoke_thunk( wasm_exec_env_t exec_env,
                                int32_t xid,
                                char* ret_ptr,
                                uint32_t ret_len,
                                const char* arg_ptr,
                                uint32_t arg_len,
                                uint32_t* bytes_written ) noexcept
  {
    const auto user_data = wasm_runtime_get_function_attachment( exec_env );
    assert( user_data );
    iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
    uint32_t retval      = 0;

    try
    {
      KOINOS_ASSERT( ret_ptr != nullptr, wasm_memory_exception, "invalid ret_ptr in invoke_thunk()" );
      KOINOS_ASSERT( arg_ptr != nullptr, wasm_memory_exception, "invalid arg_ptr in invoke_thunk()" );
      KOINOS_ASSERT( bytes_written != nullptr, wasm_memory_exception, "invalid bytes_written in invoke_thunk()" );

      retval = runner->_hapi.invoke_thunk( xid, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
    }
    catch( const std::exception& e )
    {
      wasm_runtime_set_exception( runner->_instance, e.what() );
    }

    return retval;
  }

  static uint32_t invoke_system_call( wasm_exec_env_t exec_env,
                                      int32_t xid,
                                      char* ret_ptr,
                                      uint32_t ret_len,
                                      const char* arg_ptr,
                                      uint32_t arg_len,
                                      uint32_t* bytes_written ) noexcept
  {
    const auto user_data = wasm_runtime_get_function_attachment( exec_env );
    assert( user_data );
    iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
    uint32_t retval      = 0;

    try
    {
      KOINOS_ASSERT( ret_ptr != nullptr, wasm_memory_exception, "invalid ret_ptr in invoke_system_call()" );
      KOINOS_ASSERT( arg_ptr != nullptr, wasm_memory_exception, "invalid arg_ptr in invoke_system_call()" );
      KOINOS_ASSERT( bytes_written != nullptr, wasm_memory_exception, "invalid bytes_written in invoke_system_call()" );

      retval = runner->_hapi.invoke_thunk( xid, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
    }
    catch( const std::exception& e )
    {
      wasm_runtime_set_exception( runner->_instance, e.what() );
    }

    return retval;
  }

private:
  abstract_host_api& _hapi;
  module_cache& _cache;
  module_ptr _module           = nullptr;
  wasm_module_inst_t _instance = nullptr;
  int64_t _previous_ticks      = 0;
  std::exception_ptr _exception;
};

iwasm_runner::~iwasm_runner() {}

module_ptr parse_bytecode( const char* bytecode_data, size_t bytecode_size )
{
  char errbuf[ 128 ] = { '\0' };
  KOINOS_ASSERT( bytecode_data != nullptr,
                 iwasm_returned_null_exception,
                 "iwasm_instance was unexpectedly null pointer" );

  auto module = wasm_runtime_load( reinterpret_cast< uint8_t* >( const_cast< char* >( bytecode_data ) ),
                                   bytecode_size,
                                   errbuf,
                                   sizeof( errbuf ) );

  if( !module )
  {
    KOINOS_THROW( module_parse_exception, "could not parse iwasm module: ${msg}", ( "msg", errbuf ) );
  }

  return std::make_shared< const module_guard >( module );
}

void iwasm_runner::load_module( const std::string& bytecode, const std::string& id )
{
  if( id.size() )
  {
    _module = _cache.get_module( id );
    if( !_module )
    {
      _module = parse_bytecode( bytecode.data(), bytecode.size() );
      _cache.put_module( id, _module );
    }
  }
  else
  {
    _module = parse_bytecode( bytecode.data(), bytecode.size() );
  }
}

void iwasm_runner::register_natives()
{
  // clang-format off
  NativeSymbol native_symbols[] = {
    {
      "invoke_thunk",
       (void*)&iwasm_runner::invoke_thunk,
      "(i*~*~r)i",
      this
    },
    {
      "invoke_system_call",
      (void*)&iwasm_runner::invoke_system_call,
      "(i*~*~r)i",
      this
    }
  };
  // clang-format on

  if( !wasm_runtime_register_natives( "env", native_symbols, sizeof( native_symbols ) / sizeof( NativeSymbol ) ) )
  {
    KOINOS_THROW( module_instantiate_exception, "failed to register native symbols" );
  }
}

void iwasm_runner::instantiate_module()
{
  char error_buf[ 128 ] = { '\0' };
  uint32_t stack_size = 8'092, heap_size = 32'768;
  KOINOS_ASSERT( _instance == nullptr, runner_state_exception, "_instance was unexpectedly non-null" );

  _instance = wasm_runtime_instantiate( _module->get(), stack_size, heap_size, error_buf, sizeof( error_buf ) );
  if( !_instance )
  {
    KOINOS_THROW( module_instantiate_exception, "unable to instantiate wasm runtime: ${err}", ( "err", error_buf ) );
  }
}

void iwasm_runner::call_start()
{
  uint32_t stack_size = 8'092;
  auto exec_env       = wasm_runtime_create_exec_env( _instance, stack_size );
  KOINOS_ASSERT( exec_env, create_context_exception, "unable to create wasm runtime execution environment" );

  if( bool retval = wasm_application_execute_main( _instance, 0, nullptr ); !retval )
  {
    auto err = wasm_runtime_get_exception( _instance );
    KOINOS_THROW( module_start_exception, "unable to execute main: ${err}", ( "err", err ) );
  }

  if( auto err = wasm_runtime_get_exception( _instance ); err )
  {
    KOINOS_THROW( wasm_trap_exception, "module exited due to exception: ${err}", ( "err", err ) );
  }
}

void iwasm_vm_backend::run( abstract_host_api& hapi, const std::string& bytecode, const std::string& id )
{
  iwasm_runner runner( hapi, _cache );
  runner.register_natives();
  runner.load_module( bytecode, id );
  runner.instantiate_module();
  runner.call_start();
}

} // namespace koinos::vm_manager::iwasm
