
#include <fizzy/fizzy.h>

#include <koinos/exception.hpp>

#include <koinos/vm_manager/fizzy/exceptions.hpp>
#include <koinos/vm_manager/fizzy/fizzy_vm_backend.hpp>
#include <koinos/vm_manager/timer.hpp>

#include <exception>
#include <iostream>
#include <optional>
#include <string>

#include <koinos/util/hex.hpp>

namespace koinos::vm_manager::fizzy {

namespace constants {
constexpr uint32_t fizzy_max_call_depth = 251;
constexpr std::size_t module_cache_size = 32;
} // namespace constants

/**
 * Convert a pointer from inside the VM to a native pointer.
 */
char* resolve_ptr( FizzyInstance* fizzy_instance, uint32_t ptr, uint32_t size )
{
  KOINOS_ASSERT( fizzy_instance != nullptr, null_argument_exception, "fizzy_instance was unexpectedly null pointer" );
  std::size_t mem_size = fizzy_get_instance_memory_size( fizzy_instance );
  char* mem_data       = (char*)fizzy_get_instance_memory_data( fizzy_instance );
  KOINOS_ASSERT( mem_data != nullptr,
                 fizzy_returned_null_exception,
                 "fizzy_get_instance_memory_data() unexpectedly returned null pointer" );

  if( ptr == mem_size )
  {
    if( size == 0 )
      return mem_data + mem_size;
  }
  else if( ptr > mem_size )
  {
    return nullptr;
  }

  // How much memory exists between pointer and end of memory?
  std::size_t mem_at_ptr = mem_size - ptr;
  if( mem_at_ptr < std::size_t( size ) )
    return nullptr;

  return mem_data + ptr;
}

fizzy_vm_backend::fizzy_vm_backend():
    _cache( constants::module_cache_size )
{}

fizzy_vm_backend::~fizzy_vm_backend() {}

std::string fizzy_vm_backend::backend_name()
{
  return "fizzy";
}

void fizzy_vm_backend::initialize() {}

std::string fizzy_error_code_name( FizzyErrorCode code ) noexcept
{
  switch( code )
  {
    case FizzySuccess:
      return "FizzySuccess";
    case FizzyErrorMalformedModule:
      return "FizzyErrorMalformedModule";
    case FizzyErrorInvalidModule:
      return "FizzyErrorInvalidModule";
    case FizzyErrorInstantiationFailed:
      return "FizzyErrorInstantiationFailed";
    case FizzyErrorMemoryAllocationFailed:
      return "FizzyErrorMemoryAllocationFailed";
    case FizzyErrorOther:
      return "FizzyErrorOther";
    default:
      return "UnknownFizzyError";
  }
}

class fizzy_runner
{
public:
  fizzy_runner( abstract_host_api& h, module_ptr m ):
      _hapi( h ),
      _module( m )
  {}

  ~fizzy_runner();

  void instantiate_module();
  void call_start();

  FizzyExecutionResult _wasi_args_get( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_args_sizes_get( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_seek( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_write( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_close( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_fdstat_get( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;

  FizzyExecutionResult _invoke_thunk( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _invoke_system_call( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;

  FizzyExecutionResult _koinos_get_caller( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_get_object( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_put_object( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_check_authority( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_log( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_exit( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_get_arguments( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;

private:
  abstract_host_api& _hapi;
  module_ptr _module                    = nullptr;
  FizzyInstance* _instance              = nullptr;
  FizzyExecutionContext* _fizzy_context = nullptr;
  int64_t _previous_ticks               = 0;
  std::exception_ptr _exception;
};

fizzy_runner::~fizzy_runner()
{
  if( _instance != nullptr )
    fizzy_free_instance( _instance );

  if( _fizzy_context != nullptr )
    fizzy_free_execution_context( _fizzy_context );
}

module_ptr parse_bytecode( const char* bytecode_data, size_t bytecode_size )
{
  FizzyError fizzy_err;
  KOINOS_ASSERT( bytecode_data != nullptr,
                 fizzy_returned_null_exception,
                 "fizzy_instance was unexpectedly null pointer" );
  auto ptr = fizzy_parse( reinterpret_cast< const uint8_t* >( bytecode_data ), bytecode_size, &fizzy_err );

  if( ptr == nullptr )
  {
    std::string error_code    = fizzy_error_code_name( fizzy_err.code );
    std::string error_message = fizzy_err.message;
    KOINOS_THROW( module_parse_exception,
                  "could not parse fizzy module - ${code}: ${msg}",
                  ( "code", error_code )( "msg", error_message ) );
  }

  return std::make_shared< const module_guard >( ptr );
}

void fizzy_runner::instantiate_module()
{
  KOINOS_TIMER( "fizzy_runner::instantiate_module" );
  // wasi args get
  FizzyExternalFn wasi_args_get = []( void* voidptr_context,
                                      FizzyInstance* fizzy_instance,
                                      const FizzyValue* args,
                                      FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_wasi_args_get( args, fizzy_context );
  };

  FizzyValueType wasi_args_get_arg_types[] = { FizzyValueTypeI32,
                                               FizzyValueTypeI32 };
  size_t wasi_args_get_num_args            = 2;
  FizzyExternalFunction wasi_args_get_fn   = {
    { FizzyValueTypeI32, wasi_args_get_arg_types, wasi_args_get_num_args },
    wasi_args_get,
    this
  };

  // wasi_args_sizes_get
  FizzyExternalFn wasi_args_sizes_get = []( void* voidptr_context,
                                            FizzyInstance* fizzy_instance,
                                            const FizzyValue* args,
                                            FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_wasi_args_sizes_get( args, fizzy_context );
  };

  FizzyValueType wasi_args_sizes_get_arg_types[] = { FizzyValueTypeI32,
                                                    FizzyValueTypeI32 };
  size_t wasi_args_sizes_get_num_args            = 2;
  FizzyExternalFunction wasi_args_sizes_get_fn   = {
    { FizzyValueTypeI32, wasi_args_sizes_get_arg_types, wasi_args_sizes_get_num_args },
    wasi_args_sizes_get,
    this
  };

  // wasi_fd_seek
  FizzyExternalFn wasi_fd_seek = []( void* voidptr_context,
                                     FizzyInstance* fizzy_instance,
                                     const FizzyValue* args,
                                     FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_wasi_fd_seek( args, fizzy_context );
  };

  FizzyValueType wasi_fd_seek_arg_types[] = { FizzyValueTypeI32,
                                              FizzyValueTypeI64,
                                              FizzyValueTypeI32,
                                              FizzyValueTypeI32 };
  size_t wasi_fd_seek_num_args            = 4;
  FizzyExternalFunction wasi_fd_seek_fn   = {
    { FizzyValueTypeI32, wasi_fd_seek_arg_types, wasi_fd_seek_num_args },
    wasi_fd_seek,
    this
  };

  // wasi fd write
  FizzyExternalFn wasi_fd_write = []( void* voidptr_context,
                                      FizzyInstance* fizzy_instance,
                                      const FizzyValue* args,
                                      FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_wasi_fd_write( args, fizzy_context );
  };

  FizzyValueType wasi_fd_write_arg_types[] = { FizzyValueTypeI32,
                                               FizzyValueTypeI32,
                                               FizzyValueTypeI32,
                                               FizzyValueTypeI32 };
  size_t wasi_fd_write_num_args            = 4;
  FizzyExternalFunction wasi_fd_write_fn   = {
    { FizzyValueTypeI32, wasi_fd_write_arg_types, wasi_fd_write_num_args },
    wasi_fd_write,
    this
  };

  // wasi fd close
  FizzyExternalFn wasi_fd_close = []( void* voidptr_context,
                                      FizzyInstance* fizzy_instance,
                                      const FizzyValue* args,
                                      FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_wasi_fd_close( args, fizzy_context );
  };

  FizzyValueType wasi_fd_close_arg_types[] = { FizzyValueTypeI32 };
  size_t wasi_fd_close_num_args            = 1;
  FizzyExternalFunction wasi_fd_close_fn   = {
    { FizzyValueTypeI32, wasi_fd_close_arg_types, wasi_fd_close_num_args },
    wasi_fd_close,
    this
  };

  // wasi fd fdstat get
  FizzyExternalFn wasi_fd_fdstat_get = []( void* voidptr_context,
                                      FizzyInstance* fizzy_instance,
                                      const FizzyValue* args,
                                      FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_wasi_fd_fdstat_get( args, fizzy_context );
  };

  FizzyValueType wasi_fd_fdstat_get_arg_types[] = { FizzyValueTypeI32,
                                                    FizzyValueTypeI32 };
  size_t wasi_fd_fdstat_get_num_args            = 2;
  FizzyExternalFunction wasi_fd_fdstat_get_fn   = {
    { FizzyValueTypeI32, wasi_fd_fdstat_get_arg_types, wasi_fd_fdstat_get_num_args },
    wasi_fd_fdstat_get,
    this
  };

  // invoke thunk
  FizzyExternalFn invoke_thunk = []( void* voidptr_context,
                                     FizzyInstance* fizzy_instance,
                                     const FizzyValue* args,
                                     FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_invoke_thunk( args, fizzy_context );
  };

  FizzyValueType invoke_thunk_arg_types[] = { FizzyValueTypeI32,
                                              FizzyValueTypeI32,
                                              FizzyValueTypeI32,
                                              FizzyValueTypeI32,
                                              FizzyValueTypeI32,
                                              FizzyValueTypeI32 };
  size_t invoke_thunk_num_args            = 6;
  FizzyExternalFunction invoke_thunk_fn   = {
    { FizzyValueTypeI32, invoke_thunk_arg_types, invoke_thunk_num_args },
    invoke_thunk,
    this
  };

  // invoke system call
  FizzyExternalFn invoke_system_call = []( void* voidptr_context,
                                           FizzyInstance* fizzy_instance,
                                           const FizzyValue* args,
                                           FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_invoke_system_call( args, fizzy_context );
  };

  FizzyValueType invoke_system_call_arg_types[] = { FizzyValueTypeI32,
                                                    FizzyValueTypeI32,
                                                    FizzyValueTypeI32,
                                                    FizzyValueTypeI32,
                                                    FizzyValueTypeI32,
                                                    FizzyValueTypeI32 };
  size_t invoke_system_call_num_args            = 6;
  FizzyExternalFunction invoke_system_call_fn   = {
    { FizzyValueTypeI32, invoke_system_call_arg_types, invoke_system_call_num_args },
    invoke_system_call,
    this
  };

  // get caller
  FizzyExternalFn koinos_get_caller = []( void* voidptr_context,
                                          FizzyInstance* fizzy_instance,
                                          const FizzyValue* args,
                                          FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_koinos_get_caller( args, fizzy_context );
  };

  FizzyValueType koinos_get_caller_arg_types[] = { FizzyValueTypeI32,
                                                   FizzyValueTypeI32 };
  size_t koinos_get_caller_num_args            = 2;
  FizzyExternalFunction koinos_get_caller_fn   = {
    { FizzyValueTypeI32, koinos_get_caller_arg_types, koinos_get_caller_num_args },
    koinos_get_caller,
    this
  };

  // get object
  FizzyExternalFn koinos_get_object = []( void* voidptr_context,
                                          FizzyInstance* fizzy_instance,
                                          const FizzyValue* args,
                                          FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_koinos_get_object( args, fizzy_context );
  };

  FizzyValueType koinos_get_object_arg_types[] = { FizzyValueTypeI32,
                                                   FizzyValueTypeI32,
                                                   FizzyValueTypeI32,
                                                   FizzyValueTypeI32,
                                                   FizzyValueTypeI32 };
  size_t koinos_get_object_num_args            = 5;
  FizzyExternalFunction koinos_get_object_fn   = {
    { FizzyValueTypeI32, koinos_get_object_arg_types, koinos_get_object_num_args },
    koinos_get_object,
    this
  };

  // put object
  FizzyExternalFn koinos_put_object = []( void* voidptr_context,
                                          FizzyInstance* fizzy_instance,
                                          const FizzyValue* args,
                                          FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_koinos_put_object( args, fizzy_context );
  };

  FizzyValueType koinos_put_object_arg_types[] = { FizzyValueTypeI32,
                                                   FizzyValueTypeI32,
                                                   FizzyValueTypeI32,
                                                   FizzyValueTypeI32,
                                                   FizzyValueTypeI32 };
  size_t koinos_put_object_num_args            = 5;
  FizzyExternalFunction koinos_put_object_fn   = {
    { FizzyValueTypeI32, koinos_put_object_arg_types, koinos_put_object_num_args },
    koinos_put_object,
    this
  };

  // check authority
  FizzyExternalFn koinos_check_authority = []( void* voidptr_context,
                                               FizzyInstance* fizzy_instance,
                                               const FizzyValue* args,
                                               FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_koinos_check_authority( args, fizzy_context );
  };

  FizzyValueType koinos_check_authority_arg_types[] = { FizzyValueTypeI32,
                                                        FizzyValueTypeI32,
                                                        FizzyValueTypeI32,
                                                        FizzyValueTypeI32,
                                                        FizzyValueTypeI32 };
  size_t koinos_check_authority_num_args            = 5;
  FizzyExternalFunction koinos_check_authority_fn   = {
    { FizzyValueTypeI32, koinos_check_authority_arg_types, koinos_check_authority_num_args },
    koinos_check_authority,
    this
  };

  // log
  FizzyExternalFn koinos_log = []( void* voidptr_context,
                                   FizzyInstance* fizzy_instance,
                                   const FizzyValue* args,
                                   FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_koinos_log( args, fizzy_context );
  };

  FizzyValueType koinos_log_arg_types[] = { FizzyValueTypeI32,
                                            FizzyValueTypeI32 };
  size_t koinos_log_num_args            = 2;
  FizzyExternalFunction koinos_log_fn   = {
    { FizzyValueTypeI32, koinos_log_arg_types, koinos_log_num_args },
    koinos_log,
    this
  };

  // exit
  FizzyExternalFn koinos_exit = []( void* voidptr_context,
                                    FizzyInstance* fizzy_instance,
                                    const FizzyValue* args,
                                    FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_koinos_exit( args, fizzy_context );
  };

  FizzyValueType koinos_exit_arg_types[] = { FizzyValueTypeI32,
                                             FizzyValueTypeI32,
                                             FizzyValueTypeI32 };
  size_t koinos_exit_num_args            = 3;
  FizzyExternalFunction koinos_exit_fn   = {
    { FizzyValueTypeI32, koinos_exit_arg_types, koinos_exit_num_args },
    koinos_exit,
    this
  };

  // get arguments
  FizzyExternalFn koinos_get_arguments = []( void* voidptr_context,
                                             FizzyInstance* fizzy_instance,
                                             const FizzyValue* args,
                                             FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_koinos_get_arguments( args, fizzy_context );
  };

  FizzyValueType koinos_get_arguments_arg_types[] = { FizzyValueTypeI32,
                                                      FizzyValueTypeI32,
                                                      FizzyValueTypeI32 };
  size_t koinos_get_arguments_num_args            = 3;
  FizzyExternalFunction koinos_get_arguments_fn   = {
    { FizzyValueTypeI32, koinos_get_arguments_arg_types, koinos_get_arguments_num_args },
    koinos_get_arguments,
    this
  };

  size_t num_host_funcs              = 15;
  FizzyImportedFunction host_funcs[] = {
    {"wasi_snapshot_preview1",               "args_get",          wasi_args_get_fn},
    {"wasi_snapshot_preview1",         "args_sizes_get",    wasi_args_sizes_get_fn},
    {"wasi_snapshot_preview1",                "fd_seek",           wasi_fd_seek_fn},
    {"wasi_snapshot_preview1",               "fd_write",          wasi_fd_write_fn},
    {"wasi_snapshot_preview1",               "fd_close",          wasi_fd_close_fn},
    {"wasi_snapshot_preview1",          "fd_fdstat_get",     wasi_fd_fdstat_get_fn},
    {"env",                              "invoke_thunk",           invoke_thunk_fn},
    {"env",                        "invoke_system_call",     invoke_system_call_fn},
    {"env",                         "koinos_get_caller",      koinos_get_caller_fn},
    {"env",                         "koinos_get_object",      koinos_get_object_fn},
    {"env",                         "koinos_put_object",      koinos_put_object_fn},
    {"env",                    "koinos_check_authority", koinos_check_authority_fn},
    {"env",                                "koinos_log",             koinos_log_fn},
    {"env",                               "koinos_exit",            koinos_exit_fn},
    {"env",                      "koinos_get_arguments",   koinos_get_arguments_fn}
  };

  FizzyError fizzy_err;

  uint32_t memory_pages_limit = 512; // Number of 64k pages allowed to allocate

  KOINOS_ASSERT( _instance == nullptr, runner_state_exception, "_instance was unexpectedly non-null" );
  _instance = fizzy_resolve_instantiate( _module->get(),
                                         host_funcs,
                                         num_host_funcs,
                                         nullptr,
                                         nullptr,
                                         nullptr,
                                         0,
                                         memory_pages_limit,
                                         &fizzy_err );
  if( _instance == nullptr )
  {
    std::string error_code    = fizzy_error_code_name( fizzy_err.code );
    std::string error_message = fizzy_err.message;
    KOINOS_THROW( module_instantiate_exception,
                  "could not instantiate module - ${code}: ${msg}",
                  ( "code", error_code )( "msg", error_message ) );
  }
}

FizzyExecutionResult fizzy_runner::_wasi_args_get( const FizzyValue* args,
                                                       FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t* argv = reinterpret_cast< uint32_t* >( resolve_ptr( _instance, args[ 0 ].i32, sizeof(uint32_t) ) );
    KOINOS_ASSERT( argv != nullptr, wasm_memory_exception, "invalid argv" );

    char* argv_buf = resolve_ptr( _instance, args[ 1 ].i32, sizeof(char) );
    KOINOS_ASSERT( argv_buf != nullptr, wasm_memory_exception, "invalid argv_buf" );

    try
    {
      uint32_t argc = 0;
      uint32_t argv_offset = args[ 1 ].i32;

      result.value.i32 = _hapi.wasi_args_get( &argc, argv, argv_buf );

      for( uint32_t i = 0; i < argc; ++i )
        argv[i] += argv_offset;

      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_wasi_args_sizes_get( const FizzyValue* args,
                                                         FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t* argc = reinterpret_cast< uint32_t* >( resolve_ptr( _instance, args[ 0 ].i32, sizeof(uint32_t) ) );
    KOINOS_ASSERT( argc != nullptr, wasm_memory_exception, "invalid argc" );

    uint32_t* argv_buf_size = reinterpret_cast< uint32_t* >( resolve_ptr( _instance, args[ 1 ].i32, sizeof(uint32_t) ) );
    KOINOS_ASSERT( argv_buf_size != nullptr, wasm_memory_exception, "invalid argv_buf_size" );

    try
    {
      result.value.i32 = _hapi.wasi_args_sizes_get( argc, argv_buf_size );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_wasi_fd_seek( const FizzyValue* args,
                                                  FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t fd     = args[0].i32;
    uint64_t offset = args[1].i64;
    uint8_t* whence = reinterpret_cast< uint8_t* >( resolve_ptr( _instance, args[ 2 ].i32, sizeof(uint8_t) ) );
    KOINOS_ASSERT( whence != nullptr, wasm_memory_exception, "invalid whence" );

    uint8_t* new_offset = reinterpret_cast< uint8_t* >( resolve_ptr( _instance, args[ 3 ].i32, sizeof(uint8_t) ) );
    KOINOS_ASSERT( new_offset != nullptr, wasm_memory_exception, "invalid new_offset" );

    try
    {
      result.value.i32 = _hapi.wasi_fd_seek( fd, offset, whence, new_offset );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_wasi_fd_write( const FizzyValue* args,
                                                  FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t fd       = args[0].i32;
    uint32_t iovs_len = args[2].i32;
    uint8_t* iovs = reinterpret_cast< uint8_t* >( resolve_ptr( _instance, args[ 1 ].i32, iovs_len ) );
    KOINOS_ASSERT( iovs != nullptr, wasm_memory_exception, "invalid iovs" );

    uint32_t* nwritten = reinterpret_cast< uint32_t* >( resolve_ptr( _instance, args[ 3 ].i32, sizeof(uint32_t) ) );
    KOINOS_ASSERT( nwritten, wasm_memory_exception, "invalid nwritten" );

    try
    {
      result.value.i32 = _hapi.wasi_fd_write( fd, iovs, iovs_len, nwritten );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_wasi_fd_close( const FizzyValue* args,
                                                       FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t fd = args[0].i32;

    try
    {
      result.value.i32 = _hapi.wasi_fd_close( fd );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_wasi_fd_fdstat_get( const FizzyValue* args,
                                                        FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t fd = args[0].i32;
    uint8_t* buf_ptr = reinterpret_cast< uint8_t* >( resolve_ptr( _instance, args[ 1 ].i32, sizeof(uint8_t) ) );
    KOINOS_ASSERT( buf_ptr != nullptr, wasm_memory_exception, "invalid buf_ptr" );

    try
    {
      result.value.i32 = _hapi.wasi_fd_fdstat_get( fd, buf_ptr );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_invoke_thunk( const FizzyValue* args,
                                                  FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t tid            = args[ 0 ].i32;
    uint32_t ret_len        = args[ 2 ].i32;
    char* ret_ptr           = resolve_ptr( _instance, args[ 1 ].i32, ret_len );
    uint32_t arg_len        = args[ 4 ].i32;
    const char* arg_ptr     = resolve_ptr( _instance, args[ 3 ].i32, arg_len );
    uint32_t* bytes_written = (uint32_t*)resolve_ptr( _instance, args[ 5 ].i32, sizeof( uint32_t ) );

    KOINOS_ASSERT( ret_ptr != nullptr, wasm_memory_exception, "invalid ret_ptr in invoke_thunk()" );
    KOINOS_ASSERT( arg_ptr != nullptr, wasm_memory_exception, "invalid arg_ptr in invoke_thunk()" );
    KOINOS_ASSERT( bytes_written != nullptr, wasm_memory_exception, "invalid bytes_written in invoke_thunk()" );

    int64_t* ticks = fizzy_get_execution_context_ticks( _fizzy_context );
    KOINOS_ASSERT( ticks != nullptr,
                   fizzy_returned_null_exception,
                   "fizzy_get_execution_context_ticks() unexpectedly returned null pointer" );
    _hapi.use_meter_ticks( uint64_t( _previous_ticks - *ticks ) );

    try
    {
      result.value.i32 = _hapi.invoke_thunk( tid, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }

    _previous_ticks = _hapi.get_meter_ticks();
    *ticks          = _previous_ticks;
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_invoke_system_call( const FizzyValue* args,
                                                        FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t xid            = args[ 0 ].i32;
    uint32_t ret_len        = args[ 2 ].i32;
    char* ret_ptr           = resolve_ptr( _instance, args[ 1 ].i32, ret_len );
    uint32_t arg_len        = args[ 4 ].i32;
    const char* arg_ptr     = resolve_ptr( _instance, args[ 3 ].i32, arg_len );
    uint32_t* bytes_written = (uint32_t*)resolve_ptr( _instance, args[ 5 ].i32, sizeof( uint32_t ) );

    KOINOS_ASSERT( ret_ptr != nullptr, wasm_memory_exception, "invalid ret_ptr in invoke_system_call()" );
    KOINOS_ASSERT( arg_ptr != nullptr, wasm_memory_exception, "invalid arg_ptr in invoke_system_call()" );
    KOINOS_ASSERT( bytes_written != nullptr, wasm_memory_exception, "invalid bytes_written in invoke_system_call()" );

    int64_t* ticks = fizzy_get_execution_context_ticks( _fizzy_context );
    KOINOS_ASSERT( ticks != nullptr,
                   fizzy_returned_null_exception,
                   "fizzy_get_execution_context_ticks() unexpectedly returned null pointer" );
    _hapi.use_meter_ticks( uint64_t( _previous_ticks - *ticks ) );

    try
    {
      result.value.i32 = _hapi.invoke_system_call( xid, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }

    _previous_ticks = _hapi.get_meter_ticks();
    *ticks          = _previous_ticks;
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_koinos_get_caller( const FizzyValue* args,
                                                       FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t* ret_len = reinterpret_cast< uint32_t* >( resolve_ptr( _instance, args[ 1 ].i32, sizeof( uint32_t ) ) );
    KOINOS_ASSERT( ret_len != nullptr, wasm_memory_exception, "invalid ret_len" );

    char* ret_ptr     = resolve_ptr( _instance, args[ 0 ].i32, *ret_len );
    KOINOS_ASSERT( ret_ptr != nullptr, wasm_memory_exception, "invalid ret_ptr" );

    try
    {
      result.value.i32 = _hapi.koinos_get_caller( ret_ptr, ret_len );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_koinos_get_object( const FizzyValue* args,
                                                       FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t id         = args[0].i32;
    uint32_t key_len    = args[2].i32;
    const char* key_ptr = resolve_ptr( _instance, args[ 1 ].i32, key_len );
    KOINOS_ASSERT( key_ptr != nullptr, wasm_memory_exception, "invalid ret_ptr" );

    uint32_t* value_len = reinterpret_cast< uint32_t* >( resolve_ptr( _instance, args[ 4 ].i32, sizeof( uint32_t ) ) );
    KOINOS_ASSERT( value_len != nullptr, wasm_memory_exception, "invalid value_len" );

    char* value_ptr = resolve_ptr( _instance, args[ 3 ].i32, *value_len );
    KOINOS_ASSERT( value_ptr != nullptr, wasm_memory_exception, "invalid value_ptr" );

    try
    {
      result.value.i32 = _hapi.koinos_get_object( id, key_ptr, key_len, value_ptr, value_len );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_koinos_put_object( const FizzyValue* args,
                                                       FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t id         = args[0].i32;
    uint32_t key_len    = args[2].i32;
    const char* key_ptr = resolve_ptr( _instance, args[ 1 ].i32, key_len );
    KOINOS_ASSERT( key_ptr != nullptr, wasm_memory_exception, "invalid ret_ptr" );

    uint32_t value_len    = args[ 4 ].i32;
    const char* value_ptr = resolve_ptr( _instance, args[ 3 ].i32, value_len );
    KOINOS_ASSERT( value_ptr != nullptr, wasm_memory_exception, "invalid value_ptr" );

    try
    {
      result.value.i32 = _hapi.koinos_put_object( id, key_ptr, key_len, value_ptr, value_len );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_koinos_check_authority( const FizzyValue* args,
                                                            FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t account_len    = args[ 1 ].i32;
    const char* account_ptr = resolve_ptr( _instance, args[ 0 ].i32, account_len );
    KOINOS_ASSERT( account_ptr != nullptr, wasm_memory_exception, "invalid account_ptr" );

    uint32_t data_len    = args[ 3 ].i32;
    const char* data_ptr = resolve_ptr( _instance, args[ 2 ].i32, data_len );
    KOINOS_ASSERT( data_ptr != nullptr, wasm_memory_exception, "invalid data_ptr" );

    bool* value = reinterpret_cast< bool* >( resolve_ptr( _instance, args[ 4 ].i32, sizeof( bool ) ) );
    KOINOS_ASSERT( value != nullptr, wasm_memory_exception, "invalid value" );

    try
    {
      result.value.i32 = _hapi.koinos_check_authority( account_ptr, account_len, data_ptr, data_len, value );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_koinos_log( const FizzyValue* args,
                                                       FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t msg_len    = args[ 1 ].i32;
    const char* msg_ptr = resolve_ptr( _instance, args[ 0 ].i32, msg_len );
    KOINOS_ASSERT( msg_ptr != nullptr, wasm_memory_exception, "invalid msg_ptr" );

    try
    {
      result.value.i32 = _hapi.koinos_log( msg_ptr, msg_len );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_koinos_exit( const FizzyValue* args,
                                                 FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    int32_t code        = args[ 0 ].i32;
    uint32_t res_len    = args[ 2 ].i32;
    const char* res_ptr = resolve_ptr( _instance, args[ 1 ].i32, res_len );
    KOINOS_ASSERT( res_ptr != nullptr, wasm_memory_exception, "invalid res_ptr" );

    try
    {
      result.value.i32 = _hapi.koinos_exit( code, res_ptr, res_len );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

FizzyExecutionResult fizzy_runner::_koinos_get_arguments( const FizzyValue* args,
                                                          FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t* entry_point = reinterpret_cast< uint32_t* >( resolve_ptr( _instance, args[ 0 ].i32, sizeof( uint32_t ) ) );
    KOINOS_ASSERT( entry_point != nullptr, wasm_memory_exception, "invalid entry_point" );

    uint32_t* args_len = reinterpret_cast< uint32_t* >( resolve_ptr( _instance, args[ 2 ].i32, sizeof( uint32_t ) ) );
    KOINOS_ASSERT( args_len != nullptr, wasm_memory_exception, "invalid args_len" );

    char* args_ptr = resolve_ptr( _instance, args[ 1 ].i32, *args_len );
    KOINOS_ASSERT( args_ptr != nullptr, wasm_memory_exception, "invalid args_ptr" );

    try
    {
      result.value.i32 = _hapi.koinos_get_arguments( entry_point, args_ptr, args_len );
      result.has_value = true;
    }
    catch( ... )
    {
      _exception = std::current_exception();
    }
  }
  catch( ... )
  {
    _exception = std::current_exception();
  }

  result.trapped = !!_exception;
  return result;
}

void fizzy_runner::call_start()
{
  KOINOS_TIMER( "fizzy_runner::call_start" );
  KOINOS_ASSERT( _fizzy_context == nullptr, runner_state_exception, "_fizzy_context was unexpectedly non-null" );
  _previous_ticks = _hapi.get_meter_ticks();
  _fizzy_context  = fizzy_create_metered_execution_context( constants::fizzy_max_call_depth, _previous_ticks );
  KOINOS_ASSERT( _fizzy_context != nullptr, create_context_exception, "could not create execution context" );

  uint32_t start_func_idx = 0;
  bool success            = fizzy_find_exported_function_index( _module->get(), "_start", &start_func_idx );
  KOINOS_ASSERT( success, module_start_exception, "module does not have _start function" );

  FizzyExecutionResult result = fizzy_execute( _instance, start_func_idx, nullptr, _fizzy_context );

  int64_t* ticks = fizzy_get_execution_context_ticks( _fizzy_context );
  KOINOS_ASSERT( ticks != nullptr,
                 fizzy_returned_null_exception,
                 "fizzy_get_execution_context_ticks() unexpectedly returned null pointer" );
  _hapi.use_meter_ticks( uint64_t( _previous_ticks - *ticks ) );

  if( _exception )
  {
    std::exception_ptr exc = _exception;
    _exception             = std::exception_ptr();
    std::rethrow_exception( exc );
  }

  if( result.trapped )
  {
    KOINOS_THROW( wasm_trap_exception, "module exited due to trap" );
  }
}

void fizzy_vm_backend::run( abstract_host_api& hapi, const std::string& bytecode, const std::string& id )
{
  module_ptr ptr;

  if( id.size() )
  {
    ptr = _cache.get_module( id );
    if( !ptr )
    {
      ptr = parse_bytecode( bytecode.data(), bytecode.size() );
      _cache.put_module( id, ptr );
    }
  }
  else
  {
    ptr = parse_bytecode( bytecode.data(), bytecode.size() );
  }

  fizzy_runner runner( hapi, ptr );
  runner.instantiate_module();
  runner.call_start();
}

} // namespace koinos::vm_manager::fizzy
