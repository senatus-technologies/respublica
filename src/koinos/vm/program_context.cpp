// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
#include "fizzy/fizzy.h"
#include <cassert>
#include <koinos/memory.hpp>
#include <koinos/vm/error.hpp>
#include <koinos/vm/program_context.hpp>

namespace koinos::vm {

void* native_pointer( FizzyInstance* instance, std::uint32_t ptr, std::uint32_t size ) noexcept
{
  if( !instance )
    return nullptr;

  std::size_t memory_size = fizzy_get_instance_memory_size( instance );
  auto memory_data        = fizzy_get_instance_memory_data( instance );

  if( !memory_data )
    return nullptr;

  if( memory_data + ptr + size > memory_data + memory_size )
    return nullptr;

  return static_cast< void* >( memory_data + ptr );
}

template< typename T >
  requires( std::is_pointer< T >::value )
T native_pointer_as( FizzyInstance* instance, std::uint32_t ptr, std::uint32_t size ) noexcept
{
  return static_cast< T >( native_pointer( instance, ptr, size ) );
}

result< std::vector< io_vector > > make_iovs( FizzyInstance* instance, std::uint32_t iovs, std::uint32_t iovs_len )
{
  std::uint32_t* iovs_ptr =
    native_pointer_as< std::uint32_t* >( instance, iovs, sizeof( std::uint32_t ) * 2 * iovs_len );
  if( !iovs_ptr )
    return std::unexpected( virtual_machine_errc::invalid_pointer );

  std::vector< io_vector > io_vectors;
  io_vectors.reserve( iovs_len );
  for( std::size_t i = 0; i < iovs_len; i++ )
  {
    std::uint32_t iov_buf = iovs_ptr[ i * 2 ];
    std::uint32_t iov_len = iovs_ptr[ i * 2 + 1 ];

    auto native_address = native_pointer_as< std::byte* >( instance, iov_buf, iov_len );
    if( !native_address )
      return std::unexpected( virtual_machine_errc::invalid_pointer );

    io_vectors.emplace_back( native_address, iov_len );
  }

  return io_vectors;
}

program_context::program_context( host_api& hapi, const std::shared_ptr< module >& module ) noexcept:
    _hapi( &hapi ),
    _module( module )
{}

program_context::~program_context()
{
  if( _instance != nullptr )
    fizzy_free_instance( _instance );

  if( _context != nullptr )
    fizzy_free_execution_context( _context );
}

FizzyExecutionResult program_context::_wasi_args_get( const FizzyValue* args,
                                                      FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.trapped   = true;
  result.has_value = false;
  result.value.i32 = 0;

  std::uint32_t* argv = native_pointer_as< std::uint32_t* >( _instance, args[ 0 ].i32, sizeof( std::uint32_t ) );
  if( !argv )
    return result;

  char* argv_buf = native_pointer_as< char* >( _instance, args[ 1 ].i32, sizeof( char ) );
  if( !argv_buf )
    return result;

  std::uint32_t argc        = 0;
  std::uint32_t argv_offset = args[ 1 ].i32;

  result.value.i32 = _hapi->wasi_args_get( &argc, argv, argv_buf );

  for( std::uint32_t i = 0; i < argc; ++i )
    argv[ i ] += argv_offset;

  result.has_value = true;
  result.trapped   = false;

  return result;
}

FizzyExecutionResult program_context::_wasi_args_sizes_get( const FizzyValue* args,
                                                            FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.trapped   = true;
  result.has_value = false;
  result.value.i32 = 0;

  std::uint32_t* argc = native_pointer_as< std::uint32_t* >( _instance, args[ 0 ].i32, sizeof( std::uint32_t ) );
  if( !argc )
    return result;

  std::uint32_t* argv_buf_size =
    native_pointer_as< std::uint32_t* >( _instance, args[ 1 ].i32, sizeof( std::uint32_t ) );
  if( !argv_buf_size )
    return result;

  result.value.i32 = _hapi->wasi_args_sizes_get( argc, argv_buf_size );
  result.has_value = true;
  result.trapped   = false;

  return result;
}

FizzyExecutionResult program_context::_wasi_fd_seek( const FizzyValue* args,
                                                     FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.trapped   = true;
  result.has_value = false;
  result.value.i32 = 0;

  std::uint32_t fd     = args[ 0 ].i32;
  std::uint64_t offset = args[ 1 ].i64;
  std::uint8_t* whence = native_pointer_as< std::uint8_t* >( _instance, args[ 2 ].i32, sizeof( std::uint8_t ) );
  if( !whence )
    return result;

  std::uint8_t* new_offset = native_pointer_as< std::uint8_t* >( _instance, args[ 3 ].i32, sizeof( std::uint8_t ) );
  if( !new_offset )
    return result;

  result.value.i32 = _hapi->wasi_fd_seek( fd, offset, whence, new_offset );
  result.has_value = true;
  result.trapped   = false;

  return result;
}

FizzyExecutionResult program_context::_wasi_fd_write( const FizzyValue* args,
                                                      FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.trapped   = true;
  result.has_value = false;
  result.value.i32 = 0;

  std::uint32_t fd = args[ 0 ].i32;
  auto iovs        = make_iovs( _instance, args[ 1 ].i32, args[ 2 ].i32 );
  if( !iovs )
    return result;

  std::uint32_t* nwritten = native_pointer_as< std::uint32_t* >( _instance, args[ 3 ].i32, sizeof( std::uint32_t ) );
  if( !nwritten )
    return result;

  result.value.i32 = _hapi->wasi_fd_write( fd, *iovs, nwritten );
  result.has_value = true;
  result.trapped   = false;

  return result;
}

FizzyExecutionResult program_context::_wasi_fd_read( const FizzyValue* args,
                                                     FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.trapped   = true;
  result.has_value = false;
  result.value.i32 = 0;

  std::uint32_t fd = args[ 0 ].i32;
  auto iovs        = make_iovs( _instance, args[ 1 ].i32, args[ 2 ].i32 );
  if( !iovs )
    return result;

  std::uint32_t* nwritten = native_pointer_as< std::uint32_t* >( _instance, args[ 3 ].i32, sizeof( std::uint32_t ) );
  if( !nwritten )
    return result;

  result.value.i32 = _hapi->wasi_fd_read( fd, *iovs, nwritten );
  result.has_value = true;
  result.trapped   = false;

  return result;
}

FizzyExecutionResult program_context::_wasi_fd_close( const FizzyValue* args,
                                                      FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.trapped   = true;
  result.has_value = false;
  result.value.i32 = 0;

  std::uint32_t fd = args[ 0 ].i32;

  result.value.i32 = _hapi->wasi_fd_close( fd );
  result.has_value = true;
  result.trapped   = false;

  return result;
}

FizzyExecutionResult program_context::_wasi_fd_fdstat_get( const FizzyValue* args,
                                                           FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.trapped   = true;
  result.has_value = false;
  result.value.i32 = 0;

  std::uint32_t fd       = args[ 0 ].i32;
  std::uint32_t* buf_ptr = native_pointer_as< std::uint32_t* >( _instance, args[ 1 ].i32, sizeof( std::uint32_t ) );
  if( !buf_ptr )
    return result;

  result.value.i32 = _hapi->wasi_fd_fdstat_get( fd, buf_ptr );
  result.has_value = true;
  result.trapped   = false;

  return result;
}

FizzyExecutionResult program_context::_wasi_proc_exit( const FizzyValue* args,
                                                       FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.trapped   = true;
  result.has_value = false;
  result.value.i32 = 0;

  std::int32_t exit_code = std::bit_cast< std::int32_t >( args[ 0 ].i32 );

  _hapi->wasi_proc_exit( exit_code );
  _exit_code = exit_code;

  result.trapped = false;

  return result;
}

FizzyExecutionResult program_context::_koinos_get_caller( const FizzyValue* args,
                                                          FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.trapped   = true;
  result.has_value = false;
  result.value.i32 = 0;

  std::uint32_t* ret_len = native_pointer_as< std::uint32_t* >( _instance, args[ 1 ].i32, sizeof( std::uint32_t ) );
  if( !ret_len )
    return result;

  char* ret_ptr = native_pointer_as< char* >( _instance, args[ 0 ].i32, *ret_len );
  if( !ret_ptr )
    return result;

  result.value.i32 = _hapi->koinos_get_caller( ret_ptr, ret_len );
  result.has_value = true;
  result.trapped   = false;

  return result;
}

FizzyExecutionResult program_context::_koinos_get_object( const FizzyValue* args,
                                                          FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.trapped   = true;
  result.has_value = false;
  result.value.i32 = 0;

  std::uint32_t id      = args[ 0 ].i32;
  std::uint32_t key_len = args[ 2 ].i32;
  const char* key_ptr   = native_pointer_as< const char* >( _instance, args[ 1 ].i32, key_len );
  if( !key_ptr )
    return result;

  std::uint32_t* value_len = native_pointer_as< std::uint32_t* >( _instance, args[ 4 ].i32, sizeof( std::uint32_t ) );
  if( !value_len )
    return result;

  char* value_ptr = native_pointer_as< char* >( _instance, args[ 3 ].i32, *value_len );
  if( !value_ptr )
    return result;

  result.value.i32 = _hapi->koinos_get_object( id, key_ptr, key_len, value_ptr, value_len );
  result.has_value = true;
  result.trapped   = false;

  return result;
}

FizzyExecutionResult program_context::_koinos_put_object( const FizzyValue* args,
                                                          FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.trapped   = true;
  result.has_value = false;
  result.value.i32 = 0;

  std::uint32_t id      = args[ 0 ].i32;
  std::uint32_t key_len = args[ 2 ].i32;
  const char* key_ptr   = native_pointer_as< const char* >( _instance, args[ 1 ].i32, key_len );
  if( !key_ptr )
    return result;

  std::uint32_t value_len = args[ 4 ].i32;
  const char* value_ptr   = native_pointer_as< const char* >( _instance, args[ 3 ].i32, value_len );
  if( !value_ptr )
    return result;

  result.value.i32 = _hapi->koinos_put_object( id, key_ptr, key_len, value_ptr, value_len );
  result.has_value = true;
  result.trapped   = false;

  return result;
}

FizzyExecutionResult program_context::_koinos_check_authority( const FizzyValue* args,
                                                               FizzyExecutionContext* fizzy_context ) noexcept
{
  FizzyExecutionResult result;
  result.trapped   = true;
  result.has_value = false;
  result.value.i32 = 0;

  std::uint32_t account_len = args[ 1 ].i32;
  const char* account_ptr   = native_pointer_as< const char* >( _instance, args[ 0 ].i32, account_len );
  if( !account_ptr )
    return result;

  std::uint32_t data_len = args[ 3 ].i32;
  const char* data_ptr   = native_pointer_as< const char* >( _instance, args[ 2 ].i32, data_len );
  if( !data_ptr )
    return result;

  bool* value = native_pointer_as< bool* >( _instance, args[ 4 ].i32, sizeof( bool ) );
  if( !value )
    return result;

  result.value.i32 = _hapi->koinos_check_authority( account_ptr, account_len, data_ptr, data_len, value );
  result.has_value = true;
  result.trapped   = false;

  return result;
}

std::error_code program_context::instantiate_module() noexcept
{
  // wasi args get
  FizzyExternalFn wasi_args_get = []( void* voidptr_context,
                                      FizzyInstance* fizzy_instance,
                                      const FizzyValue* args,
                                      FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    program_context* context = static_cast< program_context* >( voidptr_context );
    return context->_wasi_args_get( args, fizzy_context );
  };

  constexpr std::size_t wasi_args_get_num_args = 2;
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
    program_context* context = static_cast< program_context* >( voidptr_context );
    return context->_wasi_args_sizes_get( args, fizzy_context );
  };

  constexpr std::size_t wasi_args_sizes_get_num_args = 2;
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
    program_context* context = static_cast< program_context* >( voidptr_context );
    return context->_wasi_fd_seek( args, fizzy_context );
  };

  constexpr std::size_t wasi_fd_seek_num_args = 4;
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
    program_context* context = static_cast< program_context* >( voidptr_context );
    return context->_wasi_fd_write( args, fizzy_context );
  };

  constexpr std::size_t wasi_fd_write_num_args = 4;
  constexpr std::array< FizzyValueType, wasi_fd_write_num_args > wasi_fd_write_arg_types{ FizzyValueTypeI32,
                                                                                          FizzyValueTypeI32,
                                                                                          FizzyValueTypeI32,
                                                                                          FizzyValueTypeI32 };
  FizzyExternalFunction wasi_fd_write_fn = {
    { FizzyValueTypeI32, wasi_fd_write_arg_types.data(), wasi_fd_write_num_args },
    wasi_fd_write,
    this
  };

  // wasi fd read
  FizzyExternalFn wasi_fd_read = []( void* voidptr_context,
                                     FizzyInstance* fizzy_instance,
                                     const FizzyValue* args,
                                     FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    program_context* context = static_cast< program_context* >( voidptr_context );
    return context->_wasi_fd_read( args, fizzy_context );
  };

  constexpr std::size_t wasi_fd_read_num_args = 4;
  constexpr std::array< FizzyValueType, wasi_fd_read_num_args > wasi_fd_read_arg_types{ FizzyValueTypeI32,
                                                                                        FizzyValueTypeI32,
                                                                                        FizzyValueTypeI32,
                                                                                        FizzyValueTypeI32 };
  FizzyExternalFunction wasi_fd_read_fn = {
    { FizzyValueTypeI32, wasi_fd_read_arg_types.data(), wasi_fd_read_num_args },
    wasi_fd_read,
    this
  };

  // wasi fd close
  FizzyExternalFn wasi_fd_close = []( void* voidptr_context,
                                      FizzyInstance* fizzy_instance,
                                      const FizzyValue* args,
                                      FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    program_context* context = static_cast< program_context* >( voidptr_context );
    return context->_wasi_fd_close( args, fizzy_context );
  };

  constexpr std::size_t wasi_fd_close_num_args = 1;
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
    program_context* context = static_cast< program_context* >( voidptr_context );
    return context->_wasi_fd_fdstat_get( args, fizzy_context );
  };

  constexpr std::size_t wasi_fd_fdstat_get_num_args = 2;
  constexpr std::array< FizzyValueType, wasi_fd_fdstat_get_num_args > wasi_fd_fdstat_get_arg_types{ FizzyValueTypeI32,
                                                                                                    FizzyValueTypeI32 };
  FizzyExternalFunction wasi_fd_fdstat_get_fn = {
    { FizzyValueTypeI32, wasi_fd_fdstat_get_arg_types.data(), wasi_fd_fdstat_get_num_args },
    wasi_fd_fdstat_get,
    this
  };

  // wasi proc exit
  FizzyExternalFn wasi_proc_exit = []( void* voidptr_context,
                                       FizzyInstance* fizzy_instance,
                                       const FizzyValue* args,
                                       FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    program_context* context = static_cast< program_context* >( voidptr_context );
    return context->_wasi_proc_exit( args, fizzy_context );
  };

  constexpr std::size_t wasi_proc_exit_num_args = 1;
  constexpr std::array< FizzyValueType, wasi_fd_close_num_args > wasi_proc_exit_arg_types{ FizzyValueTypeI32 };
  FizzyExternalFunction wasi_proc_exit_fn = {
    { FizzyValueTypeVoid, wasi_proc_exit_arg_types.data(), wasi_proc_exit_num_args },
    wasi_proc_exit,
    this
  };

  // get caller
  FizzyExternalFn koinos_get_caller = []( void* voidptr_context,
                                          FizzyInstance* fizzy_instance,
                                          const FizzyValue* args,
                                          FizzyExecutionContext* fizzy_context ) noexcept -> FizzyExecutionResult
  {
    program_context* context = static_cast< program_context* >( voidptr_context );
    return context->_koinos_get_caller( args, fizzy_context );
  };

  constexpr std::size_t koinos_get_caller_num_args = 2;
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
    program_context* context = static_cast< program_context* >( voidptr_context );
    return context->_koinos_get_object( args, fizzy_context );
  };

  constexpr std::size_t koinos_get_object_num_args = 5;
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
    program_context* context = static_cast< program_context* >( voidptr_context );
    return context->_koinos_put_object( args, fizzy_context );
  };

  constexpr std::size_t koinos_put_object_num_args = 5;
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
    program_context* context = static_cast< program_context* >( voidptr_context );
    return context->_koinos_check_authority( args, fizzy_context );
  };

  constexpr std::size_t koinos_check_authority_num_args = 5;
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

  constexpr std::size_t num_host_funcs = 12;
  std::array< FizzyImportedFunction, num_host_funcs > host_funcs{
    FizzyImportedFunction{"wasi_snapshot_preview1",               "args_get",          wasi_args_get_fn},
    FizzyImportedFunction{"wasi_snapshot_preview1",         "args_sizes_get",    wasi_args_sizes_get_fn},
    FizzyImportedFunction{"wasi_snapshot_preview1",                "fd_seek",           wasi_fd_seek_fn},
    FizzyImportedFunction{"wasi_snapshot_preview1",               "fd_write",          wasi_fd_write_fn},
    FizzyImportedFunction{"wasi_snapshot_preview1",                "fd_read",           wasi_fd_read_fn},
    FizzyImportedFunction{"wasi_snapshot_preview1",               "fd_close",          wasi_fd_close_fn},
    FizzyImportedFunction{"wasi_snapshot_preview1",          "fd_fdstat_get",     wasi_fd_fdstat_get_fn},
    FizzyImportedFunction{"wasi_snapshot_preview1",              "proc_exit",         wasi_proc_exit_fn},
    FizzyImportedFunction{                   "env",      "koinos_get_caller",      koinos_get_caller_fn},
    FizzyImportedFunction{                   "env",      "koinos_get_object",      koinos_get_object_fn},
    FizzyImportedFunction{                   "env",      "koinos_put_object",      koinos_put_object_fn},
    FizzyImportedFunction{                   "env", "koinos_check_authority", koinos_check_authority_fn}
  };

  FizzyError fizzy_err;

  constexpr std::uint32_t memory_pages_limit = 512; // Number of 64k pages allowed to allocate

  if( _instance )
    fizzy_free_instance( _instance );

  _instance = fizzy_resolve_instantiate( _module->get(),
                                         host_funcs.data(),
                                         host_funcs.size(),
                                         nullptr,
                                         nullptr,
                                         nullptr,
                                         0,
                                         memory_pages_limit,
                                         &fizzy_err );

  if( !_instance )
    return virtual_machine_errc::instantiate_failure;

  return virtual_machine_errc::ok;
}

std::error_code program_context::start() noexcept
{
  if( auto error = instantiate_module(); error )
    return error;

  if( _context )
    fizzy_free_execution_context( _context );

  std::uint32_t start_func_idx = 0;

  if( !fizzy_find_exported_function_index( _module->get(), "_start", &start_func_idx ) )
    return virtual_machine_errc::entry_point_not_found;

  FizzyExecutionResult result = fizzy_execute( _instance, start_func_idx, nullptr, _context );

  if( result.trapped )
    return virtual_machine_errc::trapped;

  return make_error_code( _exit_code );
}

} // namespace koinos::vm

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
