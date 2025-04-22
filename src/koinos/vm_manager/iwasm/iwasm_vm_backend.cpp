#include <wasm_c_api.h>
#include <wasm_export.h>

#include <koinos/log.hpp>
#include <koinos/util/hex.hpp>

#include <koinos/vm_manager/iwasm/iwasm_vm_backend.hpp>
#include <koinos/vm_manager/timer.hpp>

#include <chrono>
#include <exception>
#include <string>

using namespace std::chrono_literals;
using koinos::error::error_code;

namespace koinos::vm_manager::iwasm {

namespace constants {

constexpr uint32_t stack_size           = 8'092;
constexpr uint32_t heap_size            = 32'768;
constexpr std::size_t module_cache_size = 32;

} // namespace constants

static uint32_t wasi_args_get( wasm_exec_env_t exec_env, uint32_t* argv, char* argv_buf ) noexcept;

static uint32_t wasi_args_sizes_get( wasm_exec_env_t exec_env, uint32_t* argc, uint32_t* argv_buf_size ) noexcept;

static uint32_t
wasi_fd_seek( wasm_exec_env_t exec_env, uint32_t fd, uint64_t offset, uint8_t* whence, uint8_t* new_offset ) noexcept;

static uint32_t
wasi_fd_write( wasm_exec_env_t exec_env, uint32_t fd, uint8_t* iovs, uint32_t iovs_len, uint32_t* nwritten ) noexcept;

static uint32_t wasi_fd_close( wasm_exec_env_t exec_env, uint32_t fd ) noexcept;

static uint32_t wasi_fd_fdstat_get( wasm_exec_env_t exec_env, uint32_t fd, uint8_t* buf_ptr ) noexcept;

static int32_t koinos_get_caller( wasm_exec_env_t exec_env, char* ret_ptr, uint32_t* ret_len ) noexcept;

static int32_t koinos_get_object( wasm_exec_env_t exec_env,
                                  uint32_t id,
                                  const char* key_ptr,
                                  uint32_t key_len,
                                  char* ret_ptr,
                                  uint32_t* ret_len ) noexcept;

static int32_t koinos_put_object( wasm_exec_env_t exec_env,
                                  uint32_t id,
                                  const char* key_ptr,
                                  uint32_t key_len,
                                  const char* value_ptr,
                                  uint32_t value_len ) noexcept;

static int32_t koinos_check_authority( wasm_exec_env_t exec_env,
                                       const char* account_ptr,
                                       uint32_t account_len,
                                       const char* data_ptr,
                                       uint32_t data_len,
                                       bool* value ) noexcept;

static int32_t koinos_log( wasm_exec_env_t exec_env, const char* msg_ptr, uint32_t msg_len ) noexcept;

static int32_t koinos_exit( wasm_exec_env_t exec_env, int32_t code, const char* res_bytes, uint32_t res_len ) noexcept;

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
  static NativeSymbol wasi_symbols[] = {
    {      "args_get",       (void*)&wasi_args_get,   "(**)i"},
    {"args_sizes_get", (void*)&wasi_args_sizes_get,   "(**)i"},
    {       "fd_seek",        (void*)&wasi_fd_seek, "(iI**)i"},
    {      "fd_write",       (void*)&wasi_fd_write, "(i*~*)i"},
    {      "fd_close",       (void*)&wasi_fd_close,    "(i)i"},
    { "fd_fdstat_get",  (void*)&wasi_fd_fdstat_get,   "(i*)i"}
  };
  static NativeSymbol native_symbols[] = {
    {     "koinos_get_caller",      (void*)&koinos_get_caller,    "(**)i"},
    {     "koinos_get_object",      (void*)&koinos_get_object, "(i*~**)i"},
    {     "koinos_put_object",      (void*)&koinos_put_object, "(i*~*~)i"},
    {"koinos_check_authority", (void*)&koinos_check_authority, "(*~*~*)i"},
    {            "koinos_log",             (void*)&koinos_log,    "(*~)i"},
    {           "koinos_exit",            (void*)&koinos_exit,   "(i*~)i"}
  };
  // clang-format on

  if( !wasm_runtime_register_natives( "wasi_snapshot_preview1",
                                      wasi_symbols,
                                      sizeof( wasi_symbols ) / sizeof( NativeSymbol ) ) )
    throw std::runtime_error( "failed to register wasi symbols" );

  if( !wasm_runtime_register_natives( "env", native_symbols, sizeof( native_symbols ) / sizeof( NativeSymbol ) ) )
    throw std::runtime_error( "failed to register env symbols" );
}

class iwasm_runner
{
public:
  iwasm_runner( abstract_host_api& h, module_cache& c ):
      _hapi( h ),
      _cache( c )
  {}

  ~iwasm_runner();

  error load_module( const std::string& bytecode, const std::string& id );
  error instantiate_module();
  error call_start();

  abstract_host_api& _hapi;
  std::exception_ptr _exception;
  module_cache& _cache;
  wasm_exec_env_t _exec_env    = nullptr;
  module_ptr _module           = nullptr;
  wasm_module_inst_t _instance = nullptr;
  error_code _exit_code        = error_code::success;
};

iwasm_runner::~iwasm_runner()
{
  if( _exec_env )
    wasm_runtime_destroy_exec_env( _exec_env );

  if( _instance )
    wasm_runtime_deinstantiate( _instance );
}

error iwasm_runner::load_module( const std::string& bytecode, const std::string& id )
{
  if( auto res = _cache.get_or_create_module( id, bytecode ); res )
    _module = *res;
  else
    return res.error();

  return {};
}

error iwasm_runner::instantiate_module()
{
  KOINOS_TIMER( "iwasm_runner::instantiate_module" );
  char error_buf[ 128 ] = { '\0' };
  if( _instance )
    throw std::runtime_error( "iwasm instance non-null prior to instantiation" );

  _instance = wasm_runtime_instantiate( _module->get(),
                                        constants::stack_size,
                                        constants::heap_size,
                                        error_buf,
                                        sizeof( error_buf ) );
  if( _instance == nullptr )
  {
    return error( error_code::reversion );
  }
  return {};
}

error iwasm_runner::call_start()
{
  KOINOS_TIMER( "iwasm_runner::call_start" );
  _exec_env = wasm_runtime_create_exec_env( _instance, constants::stack_size );
  if( _exec_env == nullptr )
    return error( error_code::reversion );

  wasm_runtime_set_user_data( _exec_env, this );

  auto func = wasm_runtime_lookup_function( _instance, "_start" );
  if( func == nullptr )
    return error( error_code::reversion );

  auto retcode = wasm_runtime_call_wasm( _exec_env, func, 0, nullptr );

  if( _exception )
  {
    std::exception_ptr exce = _exception;
    _exception              = std::exception_ptr();
    std::rethrow_exception( exce );
  }

  if( !retcode )
    return error( error_code::reversion );

  return error( _exit_code );
}

error iwasm_vm_backend::run( abstract_host_api& hapi, const std::string& bytecode, const std::string& id )
{
  iwasm_runner runner( hapi, _cache );
  if( auto error = runner.load_module( bytecode, id ); error )
    return error;

  if( auto error = runner.instantiate_module(); error )
    return error;

  return runner.call_start();
}

static uint32_t wasi_args_get( wasm_exec_env_t exec_env, uint32_t* argv, char* argv_buf ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  uint32_t retval      = 0;

  try
  {
    if( argv == nullptr )
    {
      // "invalid argc"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, argv, sizeof( uint32_t ) ) )
    {
      // "invalid argc"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( argv_buf == nullptr )
    {
      // "invalid argv_buf"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    uint32_t argc        = 0;
    uint32_t argv_offset = wasm_runtime_addr_native_to_app( runner->_instance, argv_buf );

    retval = runner->_hapi.wasi_args_get( &argc, argv, argv_buf );

    for( uint32_t i = 0; i < argc; ++i )
      argv[ i ] += uint32_t( argv_offset );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static uint32_t wasi_args_sizes_get( wasm_exec_env_t exec_env, uint32_t* argc, uint32_t* argv_buf_size ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  uint32_t retval      = 0;

  try
  {
    if( argc == nullptr )
    {
      // "invalid argc"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, argc, sizeof( uint32_t ) ) )
    {
      // "invalid argc"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( argv_buf_size == nullptr )
    {
      // "invalid argv_buf_size"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, argv_buf_size, sizeof( uint32_t ) ) )
    {
      // "invalid argc"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi.wasi_args_sizes_get( argc, argv_buf_size );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static uint32_t
wasi_fd_seek( wasm_exec_env_t exec_env, uint32_t fd, uint64_t offset, uint8_t* whence, uint8_t* new_offset ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  uint32_t retval      = 0;

  try
  {
    if( whence == nullptr )
    {
      // "invalid whence"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, whence, sizeof( uint32_t ) ) )
    {
      // "invalid argc"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( new_offset == nullptr )
    {
      // "invalid new_offset"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, new_offset, sizeof( uint32_t ) ) )
    {
      // "invalid argc"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi.wasi_fd_seek( fd, offset, whence, new_offset );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static uint32_t
wasi_fd_write( wasm_exec_env_t exec_env, uint32_t fd, uint8_t* iovs, uint32_t iovs_len, uint32_t* nwritten ) noexcept
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
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( nwritten == nullptr )
    {
      // "invalid new_offset"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, nwritten, sizeof( uint32_t ) ) )
    {
      // "invalid argc"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi.wasi_fd_write( fd, iovs, iovs_len, nwritten );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static uint32_t wasi_fd_close( wasm_exec_env_t exec_env, uint32_t fd ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  uint32_t retval      = 0;

  try
  {
    retval = runner->_hapi.wasi_fd_close( fd );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static uint32_t wasi_fd_fdstat_get( wasm_exec_env_t exec_env, uint32_t fd, uint8_t* buf_ptr ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  uint32_t retval      = 0;

  try
  {
    retval = runner->_hapi.wasi_fd_close( fd );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static int32_t koinos_get_caller( wasm_exec_env_t exec_env, char* ret_ptr, uint32_t* ret_len ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  int32_t retval       = 0;

  try
  {
    if( !wasm_runtime_validate_native_addr( runner->_instance, ret_len, sizeof( uint32_t ) ) )
    {
      // "invalid ret_len"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, ret_ptr, *ret_len ) )
    {
      // "invalid ret_ptr"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi.koinos_get_caller( ret_ptr, ret_len );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static int32_t koinos_get_object( wasm_exec_env_t exec_env,
                                  uint32_t id,
                                  const char* key_ptr,
                                  uint32_t key_len,
                                  char* ret_ptr,
                                  uint32_t* ret_len ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  int32_t retval       = 0;

  try
  {
    if( key_ptr == nullptr )
    {
      // "invalid key_ptr"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( ret_ptr == nullptr )
    {
      // "invalid ret_ptr"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( !wasm_runtime_validate_native_addr( runner->_instance, ret_len, sizeof( uint32_t ) ) )
    {
      // "invalid ret_len"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi.koinos_get_object( id, key_ptr, key_len, ret_ptr, ret_len );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static int32_t koinos_put_object( wasm_exec_env_t exec_env,
                                  uint32_t id,
                                  const char* key_ptr,
                                  uint32_t key_len,
                                  const char* value_ptr,
                                  uint32_t value_len ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  int32_t retval       = 0;

  try
  {
    if( key_ptr == nullptr )
    {
      // "invalid key_ptr"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( value_ptr == nullptr )
    {
      // "invalid ret_ptr"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi.koinos_put_object( id, key_ptr, key_len, value_ptr, value_len );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static int32_t koinos_check_authority( wasm_exec_env_t exec_env,
                                       const char* account_ptr,
                                       uint32_t account_len,
                                       const char* data_ptr,
                                       uint32_t data_len,
                                       bool* value ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  int32_t retval       = 0;

  try
  {
    if( account_ptr == nullptr )
    {
      // "invalid key_ptr"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( data_ptr == nullptr )
    {
      // "invalid ret_ptr"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi.koinos_check_authority( account_ptr, account_len, data_ptr, data_len, value );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static int32_t koinos_log( wasm_exec_env_t exec_env, const char* msg_ptr, uint32_t msg_len ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  int32_t retval       = 0;

  try
  {
    if( msg_ptr == nullptr )
    {
      // "invalid msg_ptr"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( msg_ptr == nullptr )
    {
      // "invalid msg_ptr"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    retval = runner->_hapi.koinos_log( msg_ptr, msg_len );
  }
  catch( const std::exception& e )
  {
    wasm_runtime_set_exception( runner->_instance, e.what() );
  }

  return retval;
}

static int32_t koinos_exit( wasm_exec_env_t exec_env, int32_t code, const char* res_bytes, uint32_t res_len ) noexcept
{
  const auto user_data = wasm_runtime_get_user_data( exec_env );
  assert( user_data );
  iwasm_runner* runner = static_cast< iwasm_runner* >( user_data );
  int32_t retval       = 0;

  try
  {
    if( res_bytes == nullptr )
    {
      // "invalid key_ptr"
      runner->_exit_code = error_code::reversion;
      wasm_runtime_terminate( runner->_instance );
      return 1;
    }

    if( code )
      runner->_exit_code = error_code::reversion;
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
