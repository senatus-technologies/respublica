#include <wasm_c_api.h>
#include <wasm_export.h>

#include <koinos/exception.hpp>
#include <koinos/vm_manager/iwasm/exceptions.hpp>
#include <koinos/vm_manager/iwasm/iwasm_vm_backend.hpp>

#include <exception>
#include <string>

namespace koinos::vm_manager::iwasm {

namespace constants {

constexpr uint32_t stack_size           = 8'092;
constexpr uint32_t heap_size            = 32'768;
constexpr std::size_t module_cache_size = 32;

} // namespace constants

static uint32_t invoke_thunk( wasm_exec_env_t exec_env,
                              int32_t xid,
                              char* ret_ptr,
                              uint32_t ret_len,
                              const char* arg_ptr,
                              uint32_t arg_len,
                              uint32_t* bytes_written ) noexcept;

static uint32_t invoke_system_call( wasm_exec_env_t exec_env,
                                    int32_t xid,
                                    char* ret_ptr,
                                    uint32_t ret_len,
                                    const char* arg_ptr,
                                    uint32_t arg_len,
                                    uint32_t* bytes_written ) noexcept;

iwasm_vm_backend::iwasm_vm_backend():
    _cache( constants::module_cache_size )
{
  wasm_runtime_init();
}

iwasm_vm_backend::~iwasm_vm_backend()
{
  wasm_runtime_destroy();
}

std::string iwasm_vm_backend::backend_name()
{
  return "iwasm";
}

void iwasm_vm_backend::initialize()
{
  // clang-format off
  static NativeSymbol native_symbols[] = {
    {
      "invoke_thunk",
       (void*)&invoke_thunk,
      "(i*~*~*)i"
    },
    {
      "invoke_system_call",
      (void*)&invoke_system_call,
      "(i*~*~*)i"
    }
  };
  // clang-format on

  if( !wasm_runtime_register_natives( "env", native_symbols, sizeof( native_symbols ) / sizeof( NativeSymbol ) ) )
  {
    KOINOS_THROW( iwasm_vm_exception, "failed to register native symbols" );
  }
}

class iwasm_runner
{
public:
  iwasm_runner( abstract_host_api& h, module_cache& c ):
      _hapi( h ),
      _cache( c )
  {}

  ~iwasm_runner();

  void load_module( const std::string& bytecode, const std::string& id );
  void instantiate_module();
  void call_start();

  abstract_host_api& _hapi;
  std::exception_ptr _exception;
  module_cache& _cache;
  wasm_exec_env_t _exec_env    = nullptr;
  module_ptr _module           = nullptr;
  wasm_module_inst_t _instance = nullptr;
  int64_t _previous_ticks      = 0;
};

iwasm_runner::~iwasm_runner()
{
  if( _exec_env )
    wasm_runtime_destroy_exec_env( _exec_env );

  if( _instance )
    wasm_runtime_deinstantiate( _instance );
}

void iwasm_runner::load_module( const std::string& bytecode, const std::string& id )
{
  _module = _cache.get_or_create_module( id, bytecode );
}

void iwasm_runner::instantiate_module()
{
  char error_buf[ 128 ] = { '\0' };
  KOINOS_ASSERT( _instance == nullptr, runner_state_exception, "_instance was unexpectedly non-null" );

  _instance =
    wasm_runtime_instantiate( _module->get(), constants::stack_size, constants::heap_size, error_buf, sizeof( error_buf ) );
  if( !_instance )
  {
    KOINOS_THROW( module_instantiate_exception, "unable to instantiate wasm runtime: ${err}", ( "err", error_buf ) );
  }
}

void iwasm_runner::call_start()
{
  _exec_env = wasm_runtime_create_exec_env( _instance, constants::stack_size );
  KOINOS_ASSERT( _exec_env, create_context_exception, "unable to create wasm runtime execution environment" );

  wasm_runtime_set_user_data( _exec_env, this );

  auto func = wasm_runtime_lookup_function( _instance, "_start" );
  KOINOS_ASSERT( func, module_start_exception, "unable to lookup _start()" );

  auto retcode = wasm_runtime_call_wasm( _exec_env, func, 0, nullptr );

  if( _exception )
  {
    std::exception_ptr exce = _exception;
    _exception              = std::exception_ptr();
    std::rethrow_exception( exce );
  }

  if( !retcode )
  {
    if( auto err = wasm_runtime_get_exception( _instance ); err )
      KOINOS_THROW( wasm_trap_exception, "module exited due to exception: ${err}", ( "err", err ) );
    else
      KOINOS_THROW( wasm_trap_exception, "module exited due to exception" );
  }
}

void iwasm_vm_backend::run( abstract_host_api& hapi, const std::string& bytecode, const std::string& id )
{
  iwasm_runner runner( hapi, _cache );
  runner.load_module( bytecode, id );
  runner.instantiate_module();
  runner.call_start();
}

static uint32_t invoke_thunk( wasm_exec_env_t exec_env,
                              int32_t xid,
                              char* ret_ptr,
                              uint32_t ret_len,
                              const char* arg_ptr,
                              uint32_t arg_len,
                              uint32_t* bytes_written ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
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
  catch( ... )
  {
    runner->_exception = std::current_exception();
    wasm_runtime_set_exception( runner->_instance, "module exit due to trap" );
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
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  uint32_t retval      = 0;

  try
  {
    KOINOS_ASSERT( ret_ptr != nullptr, wasm_memory_exception, "invalid ret_ptr in invoke_system_call()" );
    KOINOS_ASSERT( arg_ptr != nullptr, wasm_memory_exception, "invalid arg_ptr in invoke_system_call()" );
    KOINOS_ASSERT( bytes_written != nullptr, wasm_memory_exception, "invalid bytes_written in invoke_system_call()" );

    retval = runner->_hapi.invoke_system_call( xid, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
  }
  catch( ... )
  {
    runner->_exception = std::current_exception();
    wasm_runtime_set_exception( runner->_instance, "module exit due to trap" );
  }

  return retval;
}

} // namespace koinos::vm_manager::iwasm
