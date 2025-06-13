#include <wasm_c_api.h>
#include <wasm_export.h>

#include <koinos/util/memory.hpp>
#include <koinos/vm_manager/error.hpp>
#include <koinos/vm_manager/iwasm/iwasm_vm_backend.hpp>

#include <exception>
#include <string>

using namespace std::chrono_literals;

namespace koinos::vm_manager::iwasm {

namespace constants {

constexpr uint32_t stack_size           = 8'092;
constexpr uint32_t heap_size            = 32'768;
constexpr std::size_t module_cache_size = 32;

} // namespace constants

static std::uint32_t wasi_args_get( wasm_exec_env_t exec_env, std::uint32_t* argv, char* argv_buf ) noexcept;

static std::uint32_t
wasi_args_sizes_get( wasm_exec_env_t exec_env, std::uint32_t* argc, std::uint32_t* argv_buf_size ) noexcept;

static std::uint32_t wasi_fd_seek( wasm_exec_env_t exec_env,
                                   std::uint32_t fd,
                                   std::uint64_t offset,
                                   std::uint8_t* whence,
                                   std::uint8_t* new_offset ) noexcept;

static std::uint32_t wasi_fd_write( wasm_exec_env_t exec_env,
                                    std::uint32_t fd,
                                    std::uint8_t* iovs,
                                    std::uint32_t iovs_len,
                                    std::uint32_t* nwritten ) noexcept;

static std::uint32_t wasi_fd_close( wasm_exec_env_t exec_env, std::uint32_t fd ) noexcept;

static std::uint32_t wasi_fd_fdstat_get( wasm_exec_env_t exec_env, std::uint32_t fd, std::uint8_t* buf_ptr ) noexcept;

static std::int32_t koinos_get_caller( wasm_exec_env_t exec_env, char* ret_ptr, std::uint32_t* ret_len ) noexcept;

static std::int32_t koinos_get_object( wasm_exec_env_t exec_env,
                                       std::uint32_t id,
                                       const char* key_ptr,
                                       std::uint32_t key_len,
                                       char* ret_ptr,
                                       std::uint32_t* ret_len ) noexcept;

static std::int32_t koinos_put_object( wasm_exec_env_t exec_env,
                                       std::uint32_t id,
                                       const char* key_ptr,
                                       std::uint32_t key_len,
                                       const char* value_ptr,
                                       std::uint32_t value_len ) noexcept;

static std::int32_t koinos_check_authority( wasm_exec_env_t exec_env,
                                            const char* account_ptr,
                                            std::uint32_t account_len,
                                            const char* data_ptr,
                                            std::uint32_t data_len,
                                            bool* value ) noexcept;

static std::int32_t koinos_log( wasm_exec_env_t exec_env, const char* msg_ptr, std::uint32_t msg_len ) noexcept;

static std::int32_t
koinos_exit( wasm_exec_env_t exec_env, std::int32_t code, const char* res_bytes, std::uint32_t res_len ) noexcept;

iwasm_vm_backend::iwasm_vm_backend():
    _cache( constants::module_cache_size )
{
  wasm_runtime_init();
}

iwasm_vm_backend::~iwasm_vm_backend()
{
  _cache.clear();
  wasm_runtime_destroy();
}

std::string iwasm_vm_backend::backend_name()
{
  return "iwasm";
}

void iwasm_vm_backend::initialize()
{
  // NOLINTBEGIN
  constexpr size_t num_wasi_symbols = 6;
  static std::array< NativeSymbol, num_wasi_symbols > wasi_symbols{
    NativeSymbol{      "args_get",       util::pointer_cast< void* >( wasi_args_get ),   "(**)i"},
    NativeSymbol{"args_sizes_get", util::pointer_cast< void* >( wasi_args_sizes_get ),   "(**)i"},
    NativeSymbol{       "fd_seek",        util::pointer_cast< void* >( wasi_fd_seek ), "(iI**)i"},
    NativeSymbol{      "fd_write",       util::pointer_cast< void* >( wasi_fd_write ), "(i*~*)i"},
    NativeSymbol{      "fd_close",       util::pointer_cast< void* >( wasi_fd_close ),    "(i)i"},
    NativeSymbol{ "fd_fdstat_get",  util::pointer_cast< void* >( wasi_fd_fdstat_get ),   "(i*)i"}
  };

  constexpr size_t num_native_symbols = 6;
  static std::array< NativeSymbol, num_native_symbols > native_symbols{
    NativeSymbol{     "koinos_get_caller",      util::pointer_cast< void* >( koinos_get_caller ),    "(**)i"},
    NativeSymbol{     "koinos_get_object",      util::pointer_cast< void* >( koinos_get_object ), "(i*~**)i"},
    NativeSymbol{     "koinos_put_object",      util::pointer_cast< void* >( koinos_put_object ), "(i*~*~)i"},
    NativeSymbol{"koinos_check_authority", util::pointer_cast< void* >( koinos_check_authority ), "(*~*~*)i"},
    NativeSymbol{            "koinos_log",             util::pointer_cast< void* >( koinos_log ),    "(*~)i"},
    NativeSymbol{           "koinos_exit",            util::pointer_cast< void* >( koinos_exit ),   "(i*~)i"}
  };
  // NOLINTEND

  if( !wasm_runtime_register_natives( "wasi_snapshot_preview1", wasi_symbols.data(), num_wasi_symbols ) )
    throw std::runtime_error( "failed to register wasi symbols" );

  if( !wasm_runtime_register_natives( "env", native_symbols.data(), num_native_symbols ) )
    throw std::runtime_error( "failed to register env symbols" );
}

class iwasm_runner
{
public:
  iwasm_runner()                      = delete;
  iwasm_runner( const iwasm_runner& ) = delete;
  iwasm_runner( iwasm_runner&& )      = delete;
  iwasm_runner( abstract_host_api& hapi, module_cache& cache ) noexcept;

  ~iwasm_runner();

  iwasm_runner& operator=( const iwasm_runner& ) = delete;
  iwasm_runner& operator=( iwasm_runner&& )      = delete;

  std::error_code load_module( std::span< const std::byte > bytecode, std::span< const std::byte > id );
  std::error_code instantiate_module();
  std::error_code call_start();

  abstract_host_api* _hapi;
  module_cache* _cache;
  std::exception_ptr _exception;
  wasm_exec_env_t _exec_env    = nullptr;
  module_ptr _module           = nullptr;
  wasm_module_inst_t _instance = nullptr;
  std::error_code _exit_code   = virtual_machine_errc::ok;
};

iwasm_runner::iwasm_runner( abstract_host_api& hapi, module_cache& cache ) noexcept:
    _hapi( &hapi ),
    _cache( &cache )
{}

iwasm_runner::~iwasm_runner()
{
  if( _exec_env )
    wasm_runtime_destroy_exec_env( _exec_env );

  if( _instance )
    wasm_runtime_deinstantiate( _instance );
}

std::error_code iwasm_runner::load_module( std::span< const std::byte > bytecode, std::span< const std::byte > id )
{
  if( auto res = _cache->get_or_create_module( id, bytecode ); res )
    _module = *res;
  else
    return res.error();

  return virtual_machine_errc::ok;
}

std::error_code iwasm_runner::instantiate_module()
{
  constexpr size_t error_buf_size = 128;
  std::array< char, error_buf_size > error_buf{ '\0' };
  if( _instance )
    throw std::runtime_error( "iwasm instance non-null prior to instantiation" );

  _instance = wasm_runtime_instantiate( _module->get(),
                                        constants::stack_size,
                                        constants::heap_size,
                                        error_buf.data(),
                                        error_buf.size() );
  if( _instance == nullptr )
    return virtual_machine_errc::instantiate_failure;

  return virtual_machine_errc::ok;
}

std::error_code iwasm_runner::call_start()
{
  _exec_env = wasm_runtime_create_exec_env( _instance, constants::stack_size );
  if( _exec_env == nullptr )
    return virtual_machine_errc::execution_environment_failure;

  wasm_runtime_set_user_data( _exec_env, this );

  auto func = wasm_runtime_lookup_function( _instance, "_start" );
  if( func == nullptr )
    return virtual_machine_errc::function_lookup_failure;

  auto retcode = wasm_runtime_call_wasm( _exec_env, func, 0, nullptr );

  if( _exception )
  {
    std::exception_ptr exce = _exception;
    _exception              = std::exception_ptr();
    std::rethrow_exception( exce );
  }

  if( !retcode )
    return virtual_machine_errc::trapped;

  return _exit_code;
}

std::error_code
iwasm_vm_backend::run( abstract_host_api& hapi, std::span< const std::byte > bytecode, std::span< const std::byte > id )
{
  iwasm_runner runner( hapi, _cache );
  if( auto error = runner.load_module( bytecode, id ); error )
    return error;

  if( auto error = runner.instantiate_module(); error )
    return error;

  return runner.call_start();
}

static std::uint32_t wasi_args_get( wasm_exec_env_t exec_env, std::uint32_t* argv, char* argv_buf ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  std::uint32_t retval = 0;

  try
  {
    if( argv == nullptr )
    {
      // "invalid argc"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, argv, sizeof( uint32_t ) ) )
    {
      // "invalid argc"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( argv_buf == nullptr )
    {
      // "invalid argv_buf"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    std::uint32_t argc        = 0;
    std::uint32_t argv_offset = wasm_runtime_addr_native_to_app( runner->_instance, argv_buf );

    retval = runner->_hapi->wasi_args_get( &argc, argv, argv_buf );

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic) We need pointer arithmetic to offset from argv
    for( uint32_t i = 0; i < argc; ++i )
      argv[ i ] += argv_offset;
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static std::uint32_t
wasi_args_sizes_get( wasm_exec_env_t exec_env, std::uint32_t* argc, std::uint32_t* argv_buf_size ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  std::uint32_t retval = 0;

  try
  {
    if( argc == nullptr )
    {
      // "invalid argc"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, argc, sizeof( uint32_t ) ) )
    {
      // "invalid argc"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( argv_buf_size == nullptr )
    {
      // "invalid argv_buf_size"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, argv_buf_size, sizeof( uint32_t ) ) )
    {
      // "invalid argc"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi->wasi_args_sizes_get( argc, argv_buf_size );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static std::uint32_t wasi_fd_seek( wasm_exec_env_t exec_env,
                                   std::uint32_t fd,
                                   std::uint64_t offset,
                                   std::uint8_t* whence,
                                   std::uint8_t* new_offset ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  std::uint32_t retval = 0;

  try
  {
    if( whence == nullptr )
    {
      // "invalid whence"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, whence, sizeof( uint32_t ) ) )
    {
      // "invalid argc"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( new_offset == nullptr )
    {
      // "invalid new_offset"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, new_offset, sizeof( uint32_t ) ) )
    {
      // "invalid argc"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi->wasi_fd_seek( fd, offset, whence, new_offset );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static std::uint32_t wasi_fd_write( wasm_exec_env_t exec_env,
                                    std::uint32_t fd,
                                    std::uint8_t* iovs,
                                    std::uint32_t iovs_len,
                                    std::uint32_t* nwritten ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  uint32_t retval      = 0;

  try
  {
    if( iovs == nullptr )
    {
      // "invalid whence"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( nwritten == nullptr )
    {
      // "invalid new_offset"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, nwritten, sizeof( uint32_t ) ) )
    {
      // "invalid argc"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi->wasi_fd_write( fd, iovs, iovs_len, nwritten );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static std::uint32_t wasi_fd_close( wasm_exec_env_t exec_env, std::uint32_t fd ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  std::uint32_t retval = 0;

  try
  {
    retval = runner->_hapi->wasi_fd_close( fd );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static std::uint32_t wasi_fd_fdstat_get( wasm_exec_env_t exec_env, std::uint32_t fd, std::uint8_t* buf_ptr ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  uint32_t retval      = 0;

  try
  {
    retval = runner->_hapi->wasi_fd_close( fd );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static std::int32_t koinos_get_caller( wasm_exec_env_t exec_env, char* ret_ptr, std::uint32_t* ret_len ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  int32_t retval       = 0;

  try
  {
    if( !wasm_runtime_validate_native_addr( runner->_instance, ret_len, sizeof( std::uint32_t ) ) )
    {
      // "invalid ret_len"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, ret_ptr, *ret_len ) )
    {
      // "invalid ret_ptr"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi->koinos_get_caller( ret_ptr, ret_len );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static std::int32_t koinos_get_object( wasm_exec_env_t exec_env,
                                       std::uint32_t id,
                                       const char* key_ptr,
                                       std::uint32_t key_len,
                                       char* ret_ptr,
                                       std::uint32_t* ret_len ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  std::int32_t retval  = 0;

  try
  {
    if( key_ptr == nullptr )
    {
      // "invalid key_ptr"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( ret_ptr == nullptr )
    {
      // "invalid ret_ptr"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, ret_len, sizeof( uint32_t ) ) )
    {
      // "invalid ret_len"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi->koinos_get_object( id, key_ptr, key_len, ret_ptr, ret_len );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static std::int32_t koinos_put_object( wasm_exec_env_t exec_env,
                                       std::uint32_t id,
                                       const char* key_ptr,
                                       std::uint32_t key_len,
                                       const char* value_ptr,
                                       std::uint32_t value_len ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  std::int32_t retval  = 0;

  try
  {
    if( key_ptr == nullptr )
    {
      // "invalid key_ptr"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( value_ptr == nullptr )
    {
      // "invalid ret_ptr"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi->koinos_put_object( id, key_ptr, key_len, value_ptr, value_len );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static std::int32_t koinos_check_authority( wasm_exec_env_t exec_env,
                                            const char* account_ptr,
                                            std::uint32_t account_len,
                                            const char* data_ptr,
                                            std::uint32_t data_len,
                                            bool* value ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  std::int32_t retval  = 0;

  try
  {
    if( account_ptr == nullptr )
    {
      // "invalid key_ptr"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( data_ptr == nullptr )
    {
      // "invalid ret_ptr"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi->koinos_check_authority( account_ptr, account_len, data_ptr, data_len, value );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static std::int32_t koinos_log( wasm_exec_env_t exec_env, const char* msg_ptr, std::uint32_t msg_len ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  std::int32_t retval  = 0;

  try
  {
    if( msg_ptr == nullptr )
    {
      // "invalid msg_ptr"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( msg_ptr == nullptr )
    {
      // "invalid msg_ptr"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi->koinos_log( msg_ptr, msg_len );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static std::int32_t
koinos_exit( wasm_exec_env_t exec_env, std::int32_t code, const char* res_bytes, std::uint32_t res_len ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  std::int32_t retval  = 0;

  try
  {
    if( res_bytes == nullptr )
    {
      // "invalid key_ptr"
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( code )
      runner->_exit_code = virtual_machine_errc::invalid_arguments;
    wasm_runtime_terminate( runner->_instance );
    return 0;
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

} // namespace koinos::vm_manager::iwasm
