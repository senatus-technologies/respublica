// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// The Fizzy VM is inherently low level and must do pointer arithmetic to convert
// between WASM and native pointers.

#include <fizzy/fizzy.h>

#include <koinos/vm_manager/error.hpp>
#include <koinos/vm_manager/fizzy/fizzy_vm_backend.hpp>

#include <exception>
#include <string>

#include <koinos/util/memory.hpp>

namespace koinos::vm_manager::fizzy {

/**
 * Convert a pointer from inside the VM to a native pointer.
 */
char* resolve_ptr( FizzyInstance* fizzy_instance, uint32_t ptr, uint32_t size )
{
  if( fizzy_instance == nullptr )
    throw std::runtime_error( "fizzy_instance was unexpectedly null pointer" );
  std::size_t mem_size = fizzy_get_instance_memory_size( fizzy_instance );
  auto mem_data        = util::pointer_cast< char* >( fizzy_get_instance_memory_data( fizzy_instance ) );
  if( mem_data == nullptr )
    throw std::runtime_error( "" );

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
  fizzy_runner()                      = delete;
  fizzy_runner( const fizzy_runner& ) = delete;
  fizzy_runner( fizzy_runner&& )      = delete;
  fizzy_runner( abstract_host_api& h, const module_ptr& m ) noexcept;

  ~fizzy_runner();

  fizzy_runner& operator=( const fizzy_runner& ) = delete;
  fizzy_runner& operator=( fizzy_runner&& )      = delete;

  void instantiate_module();
  std::error_code call_start();

  FizzyExecutionResult _wasi_args_get( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_args_sizes_get( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_seek( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_write( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_close( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _wasi_fd_fdstat_get( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;

  FizzyExecutionResult _koinos_get_caller( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_get_object( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_put_object( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_check_authority( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_log( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;
  FizzyExecutionResult _koinos_exit( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept;

private:
  abstract_host_api* _hapi              = nullptr;
  module_ptr _module                    = nullptr;
  FizzyInstance* _instance              = nullptr;
  FizzyExecutionContext* _fizzy_context = nullptr;
  std::exception_ptr _exception;
};

fizzy_runner::fizzy_runner( abstract_host_api& hapi, const module_ptr& module ) noexcept:
    _hapi( &hapi ),
    _module( module )
{}

fizzy_runner::~fizzy_runner()
{
  if( _instance != nullptr )
    fizzy_free_instance( _instance );

  if( _fizzy_context != nullptr )
    fizzy_free_execution_context( _fizzy_context );
}

module_ptr parse_bytecode( std::span< const std::byte > bytecode )
{
  FizzyError fizzy_err;
  if( bytecode.size() == 0 )
    throw std::runtime_error( "" );
  auto ptr = fizzy_parse( util::pointer_cast< const uint8_t* >( bytecode.data() ), bytecode.size(), &fizzy_err );

  if( ptr == nullptr )
    throw std::runtime_error( "could not parse fizzy module" );

  return std::make_shared< const module_guard >( ptr );
}

void fizzy_runner::instantiate_module()
{
  // wasi args get
  FizzyExternalFn wasi_args_get = []( void* voidptr_context,
                                      FizzyInstance* fizzy_instance,
                                      const FizzyValue* args,
                                      FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    fizzy_runner* runner = static_cast< fizzy_runner* >( voidptr_context );
    return runner->_wasi_args_get( args, fizzy_context );
  };

  constexpr size_t wasi_args_get_num_args = 2;
  constexpr std::array< FizzyValueType, wasi_args_get_num_args > wasi_args_get_arg_types{ FizzyValueTypeI32,
                                                                                          FizzyValueTypeI32 };
  FizzyExternalFunction wasi_args_get_fn = {
    { FizzyValueTypeI32, wasi_args_get_arg_types.data(), wasi_args_get_num_args },
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

  constexpr size_t wasi_args_sizes_get_num_args = 2;
  constexpr std::array< FizzyValueType, wasi_args_sizes_get_num_args > wasi_args_sizes_get_arg_types{
    FizzyValueTypeI32,
    FizzyValueTypeI32 };
  FizzyExternalFunction wasi_args_sizes_get_fn = {
    { FizzyValueTypeI32, wasi_args_sizes_get_arg_types.data(), wasi_args_sizes_get_num_args },
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

  constexpr size_t wasi_fd_seek_num_args = 4;
  constexpr std::array< FizzyValueType, wasi_fd_seek_num_args > wasi_fd_seek_arg_types{ FizzyValueTypeI32,
                                                                                        FizzyValueTypeI64,
                                                                                        FizzyValueTypeI32,
                                                                                        FizzyValueTypeI32 };
  FizzyExternalFunction wasi_fd_seek_fn = {
    { FizzyValueTypeI32, wasi_fd_seek_arg_types.data(), wasi_fd_seek_num_args },
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

  constexpr size_t wasi_fd_write_num_args = 4;
  constexpr std::array< FizzyValueType, wasi_fd_write_num_args > wasi_fd_write_arg_types{ FizzyValueTypeI32,
                                                                                          FizzyValueTypeI32,
                                                                                          FizzyValueTypeI32,
                                                                                          FizzyValueTypeI32 };
  FizzyExternalFunction wasi_fd_write_fn = {
    { FizzyValueTypeI32, wasi_fd_write_arg_types.data(), wasi_fd_write_num_args },
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

  constexpr size_t wasi_fd_close_num_args = 1;
  constexpr std::array< FizzyValueType, wasi_fd_close_num_args > wasi_fd_close_arg_types{ FizzyValueTypeI32 };
  FizzyExternalFunction wasi_fd_close_fn = {
    { FizzyValueTypeI32, wasi_fd_close_arg_types.data(), wasi_fd_close_num_args },
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

  constexpr size_t wasi_fd_fdstat_get_num_args = 2;
  constexpr std::array< FizzyValueType, wasi_fd_fdstat_get_num_args > wasi_fd_fdstat_get_arg_types{ FizzyValueTypeI32,
                                                                                                    FizzyValueTypeI32 };
  FizzyExternalFunction wasi_fd_fdstat_get_fn = {
    { FizzyValueTypeI32, wasi_fd_fdstat_get_arg_types.data(), wasi_fd_fdstat_get_num_args },
    wasi_fd_fdstat_get,
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

  constexpr size_t koinos_get_caller_num_args = 2;
  constexpr std::array< FizzyValueType, koinos_get_caller_num_args > koinos_get_caller_arg_types{ FizzyValueTypeI32,
                                                                                                  FizzyValueTypeI32 };
  FizzyExternalFunction koinos_get_caller_fn = {
    { FizzyValueTypeI32, koinos_get_caller_arg_types.data(), koinos_get_caller_num_args },
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

  constexpr size_t koinos_get_object_num_args = 5;
  constexpr std::array< FizzyValueType, koinos_get_object_num_args > koinos_get_object_arg_types{ FizzyValueTypeI32,
                                                                                                  FizzyValueTypeI32,
                                                                                                  FizzyValueTypeI32,
                                                                                                  FizzyValueTypeI32,
                                                                                                  FizzyValueTypeI32 };
  FizzyExternalFunction koinos_get_object_fn = {
    { FizzyValueTypeI32, koinos_get_object_arg_types.data(), koinos_get_object_num_args },
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

  constexpr size_t koinos_put_object_num_args = 5;
  constexpr std::array< FizzyValueType, koinos_put_object_num_args > koinos_put_object_arg_types{ FizzyValueTypeI32,
                                                                                                  FizzyValueTypeI32,
                                                                                                  FizzyValueTypeI32,
                                                                                                  FizzyValueTypeI32,
                                                                                                  FizzyValueTypeI32 };
  FizzyExternalFunction koinos_put_object_fn = {
    { FizzyValueTypeI32, koinos_put_object_arg_types.data(), koinos_put_object_num_args },
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

  constexpr size_t koinos_check_authority_num_args = 5;
  constexpr std::array< FizzyValueType, koinos_check_authority_num_args > koinos_check_authority_arg_types{
    FizzyValueTypeI32,
    FizzyValueTypeI32,
    FizzyValueTypeI32,
    FizzyValueTypeI32,
    FizzyValueTypeI32 };
  FizzyExternalFunction koinos_check_authority_fn = {
    { FizzyValueTypeI32, koinos_check_authority_arg_types.data(), koinos_check_authority_num_args },
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

  constexpr size_t koinos_log_num_args = 2;
  constexpr std::array< FizzyValueType, koinos_log_num_args > koinos_log_arg_types{ FizzyValueTypeI32,
                                                                                    FizzyValueTypeI32 };
  FizzyExternalFunction koinos_log_fn = {
    { FizzyValueTypeI32, koinos_log_arg_types.data(), koinos_log_num_args },
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

  constexpr size_t koinos_exit_num_args = 3;
  constexpr std::array< FizzyValueType, koinos_exit_num_args > koinos_exit_arg_types{ FizzyValueTypeI32,
                                                                                      FizzyValueTypeI32,
                                                                                      FizzyValueTypeI32 };
  FizzyExternalFunction koinos_exit_fn = {
    { FizzyValueTypeI32, koinos_exit_arg_types.data(), koinos_exit_num_args },
    koinos_exit,
    this
  };

  constexpr size_t num_host_funcs = 12;
  std::array< FizzyImportedFunction, num_host_funcs > host_funcs{
    FizzyImportedFunction{"wasi_snapshot_preview1",               "args_get",          wasi_args_get_fn},
    FizzyImportedFunction{"wasi_snapshot_preview1",         "args_sizes_get",    wasi_args_sizes_get_fn},
    FizzyImportedFunction{"wasi_snapshot_preview1",                "fd_seek",           wasi_fd_seek_fn},
    FizzyImportedFunction{"wasi_snapshot_preview1",               "fd_write",          wasi_fd_write_fn},
    FizzyImportedFunction{"wasi_snapshot_preview1",               "fd_close",          wasi_fd_close_fn},
    FizzyImportedFunction{"wasi_snapshot_preview1",          "fd_fdstat_get",     wasi_fd_fdstat_get_fn},
    FizzyImportedFunction{                   "env",      "koinos_get_caller",      koinos_get_caller_fn},
    FizzyImportedFunction{                   "env",      "koinos_get_object",      koinos_get_object_fn},
    FizzyImportedFunction{                   "env",      "koinos_put_object",      koinos_put_object_fn},
    FizzyImportedFunction{                   "env", "koinos_check_authority", koinos_check_authority_fn},
    FizzyImportedFunction{                   "env",             "koinos_log",             koinos_log_fn},
    FizzyImportedFunction{                   "env",            "koinos_exit",            koinos_exit_fn}
  };

  FizzyError fizzy_err;

  constexpr uint32_t memory_pages_limit = 512; // Number of 64k pages allowed to allocate

  if( _instance != nullptr )
    throw std::runtime_error( "" );
  _instance = fizzy_resolve_instantiate( _module->get(),
                                         host_funcs.data(),
                                         num_host_funcs,
                                         nullptr,
                                         nullptr,
                                         nullptr,
                                         0,
                                         memory_pages_limit,
                                         &fizzy_err );
  if( _instance == nullptr )
    throw std::runtime_error( "could not instantiate module" );
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
    uint32_t* argv = util::pointer_cast< uint32_t* >( resolve_ptr( _instance, args[ 0 ].i32, sizeof( uint32_t ) ) );
    if( argv == nullptr )
      throw std::runtime_error( "" );

    char* argv_buf = resolve_ptr( _instance, args[ 1 ].i32, sizeof( char ) );
    if( argv_buf == nullptr )
      throw std::runtime_error( "" );

    try
    {
      uint32_t argc        = 0;
      uint32_t argv_offset = args[ 1 ].i32;

      result.value.i32 = _hapi->wasi_args_get( &argc, argv, argv_buf );

      for( uint32_t i = 0; i < argc; ++i )
        argv[ i ] += argv_offset;

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
    uint32_t* argc = util::pointer_cast< uint32_t* >( resolve_ptr( _instance, args[ 0 ].i32, sizeof( uint32_t ) ) );
    if( argc == nullptr )
      throw std::runtime_error( "" );

    uint32_t* argv_buf_size =
      util::pointer_cast< uint32_t* >( resolve_ptr( _instance, args[ 1 ].i32, sizeof( uint32_t ) ) );
    if( argv_buf_size == nullptr )
      throw std::runtime_error( "" );

    try
    {
      result.value.i32 = _hapi->wasi_args_sizes_get( argc, argv_buf_size );
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
    uint32_t fd     = args[ 0 ].i32;
    uint64_t offset = args[ 1 ].i64;
    uint8_t* whence = util::pointer_cast< uint8_t* >( resolve_ptr( _instance, args[ 2 ].i32, sizeof( uint8_t ) ) );
    if( whence == nullptr )
      throw std::runtime_error( "" );

    uint8_t* new_offset = util::pointer_cast< uint8_t* >( resolve_ptr( _instance, args[ 3 ].i32, sizeof( uint8_t ) ) );
    if( new_offset == nullptr )
      throw std::runtime_error( "" );

    try
    {
      result.value.i32 = _hapi->wasi_fd_seek( fd, offset, whence, new_offset );
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
    uint32_t fd       = args[ 0 ].i32;
    uint32_t iovs_len = args[ 2 ].i32;
    uint8_t* iovs     = util::pointer_cast< uint8_t* >( resolve_ptr( _instance, args[ 1 ].i32, iovs_len ) );
    if( iovs == nullptr )
      throw std::runtime_error( "" );

    uint32_t* nwritten = util::pointer_cast< uint32_t* >( resolve_ptr( _instance, args[ 3 ].i32, sizeof( uint32_t ) ) );
    if( !( nwritten ) )
      throw std::runtime_error( "" );

    try
    {
      result.value.i32 = _hapi->wasi_fd_write( fd, iovs, iovs_len, nwritten );
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
    uint32_t fd = args[ 0 ].i32;

    try
    {
      result.value.i32 = _hapi->wasi_fd_close( fd );
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
    uint32_t fd      = args[ 0 ].i32;
    uint8_t* buf_ptr = util::pointer_cast< uint8_t* >( resolve_ptr( _instance, args[ 1 ].i32, sizeof( uint8_t ) ) );
    if( !( buf_ptr != nullptr ) )
      throw std::runtime_error( "" );

    try
    {
      result.value.i32 = _hapi->wasi_fd_fdstat_get( fd, buf_ptr );
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

FizzyExecutionResult fizzy_runner::_koinos_get_caller( const FizzyValue* args,
                                                       FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t* ret_len = util::pointer_cast< uint32_t* >( resolve_ptr( _instance, args[ 1 ].i32, sizeof( uint32_t ) ) );
    if( !( ret_len != nullptr ) )
      throw std::runtime_error( "" );

    char* ret_ptr = resolve_ptr( _instance, args[ 0 ].i32, *ret_len );
    if( !( ret_ptr != nullptr ) )
      throw std::runtime_error( "" );

    try
    {
      result.value.i32 = _hapi->koinos_get_caller( ret_ptr, ret_len );
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
    uint32_t id         = args[ 0 ].i32;
    uint32_t key_len    = args[ 2 ].i32;
    const char* key_ptr = resolve_ptr( _instance, args[ 1 ].i32, key_len );
    if( !( key_ptr != nullptr ) )
      throw std::runtime_error( "" );

    uint32_t* value_len =
      util::pointer_cast< uint32_t* >( resolve_ptr( _instance, args[ 4 ].i32, sizeof( uint32_t ) ) );
    if( !( value_len != nullptr ) )
      throw std::runtime_error( "" );

    char* value_ptr = resolve_ptr( _instance, args[ 3 ].i32, *value_len );
    if( !( value_ptr != nullptr ) )
      throw std::runtime_error( "" );

    try
    {
      result.value.i32 = _hapi->koinos_get_object( id, key_ptr, key_len, value_ptr, value_len );
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
    uint32_t id         = args[ 0 ].i32;
    uint32_t key_len    = args[ 2 ].i32;
    const char* key_ptr = resolve_ptr( _instance, args[ 1 ].i32, key_len );
    if( !( key_ptr != nullptr ) )
      throw std::runtime_error( "" );

    uint32_t value_len    = args[ 4 ].i32;
    const char* value_ptr = resolve_ptr( _instance, args[ 3 ].i32, value_len );
    if( !( value_ptr != nullptr ) )
      throw std::runtime_error( "" );

    try
    {
      result.value.i32 = _hapi->koinos_put_object( id, key_ptr, key_len, value_ptr, value_len );
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
    if( !( account_ptr != nullptr ) )
      throw std::runtime_error( "" );

    uint32_t data_len    = args[ 3 ].i32;
    const char* data_ptr = resolve_ptr( _instance, args[ 2 ].i32, data_len );
    if( !( data_ptr != nullptr ) )
      throw std::runtime_error( "" );

    bool* value = util::pointer_cast< bool* >( resolve_ptr( _instance, args[ 4 ].i32, sizeof( bool ) ) );
    if( !( value != nullptr ) )
      throw std::runtime_error( "" );

    try
    {
      result.value.i32 = _hapi->koinos_check_authority( account_ptr, account_len, data_ptr, data_len, value );
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

FizzyExecutionResult fizzy_runner::_koinos_log( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    uint32_t msg_len    = args[ 1 ].i32;
    const char* msg_ptr = resolve_ptr( _instance, args[ 0 ].i32, msg_len );
    if( !( msg_ptr != nullptr ) )
      throw std::runtime_error( "" );

    try
    {
      result.value.i32 = _hapi->koinos_log( msg_ptr, msg_len );
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

FizzyExecutionResult fizzy_runner::_koinos_exit( const FizzyValue* args, FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.has_value = false;
  result.value.i32 = 0;

  _exception = std::exception_ptr();

  try
  {
    [[maybe_unused]]
    int32_t code        = std::bit_cast< int32_t >( args[ 0 ].i32 );
    uint32_t res_len    = args[ 2 ].i32;
    const char* res_ptr = resolve_ptr( _instance, args[ 1 ].i32, res_len );
    if( !( res_ptr != nullptr ) )
      throw std::runtime_error( "" );

    try
    {
      if( code )
      {
        result.value.i32 = code;
        result.has_value = true;
      }
      else
        result.trapped = true;
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

  result.trapped |= !!_exception;
  return result;
}

std::error_code fizzy_runner::call_start()
{
  if( !( _fizzy_context == nullptr ) )
    throw std::runtime_error( "" );

  uint32_t start_func_idx = 0;
  bool success            = fizzy_find_exported_function_index( _module->get(), "_start", &start_func_idx );
  if( !( success ) )
    throw std::runtime_error( "" );

  FizzyExecutionResult result = fizzy_execute( _instance, start_func_idx, nullptr, _fizzy_context );

  if( _exception )
  {
    std::exception_ptr exc = _exception;
    _exception             = std::exception_ptr();
    std::rethrow_exception( exc );
  }

  if( result.trapped )
    return virtual_machine_errc::trapped;

  return virtual_machine_errc::ok;
}

std::error_code
fizzy_vm_backend::run( abstract_host_api& hapi, std::span< const std::byte > bytecode, std::span< const std::byte > id )
{
  module_ptr ptr;

  if( id.size() )
  {
    ptr = _cache.get_module( id );
    if( !ptr )
    {
      ptr = parse_bytecode( bytecode );
      _cache.put_module( id, ptr );
    }
  }
  else
  {
    ptr = parse_bytecode( bytecode );
  }

  fizzy_runner runner( hapi, ptr );
  runner.instantiate_module();
  return runner.call_start();
}

} // namespace koinos::vm_manager::fizzy

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
