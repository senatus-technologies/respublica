
#include <koinos/controller/host_api.hpp>

#include <koinos/memory.hpp>

using namespace std::string_literals;

namespace koinos::controller {

host_api::host_api( execution_context& ctx ):
    _ctx( ctx )
{}

host_api::~host_api() {}

std::int32_t host_api::wasi_args_get( std::uint32_t* argc, std::uint32_t* argv, char* argv_buf )
{
#pragma message( "wasi_args_get can be simplified when entry points are not handled specially" )
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  auto args = _ctx.program_arguments();
  if( !args.size() )
    return static_cast< std::uint32_t >( reversion_errc::ok );

  std::uint32_t counter = 0;
  std::uint32_t index   = 0;

  argv[ index++ ] = counter;
  std::memcpy( argv_buf + counter, args[ 0 ].data(), args[ 0 ].size() );
  counter += args[ 0 ].size();

  for( std::size_t i = 1; i < args.size(); ++i )
  {
    const auto& arg    = args[ i ];
    argv[ index++ ]    = counter;
    std::uint32_t size = arg.size();
    std::memcpy( argv_buf + counter, &size, sizeof( std::uint32_t ) );
    counter += sizeof( std::uint32_t );

    argv[ index++ ] = counter;
    std::memcpy( argv_buf + counter, arg.data(), arg.size() );
    counter += arg.size();
  }

  *argc = index;

  return static_cast< std::int32_t >( reversion_errc::ok );
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

std::int32_t host_api::wasi_args_sizes_get( std::uint32_t* argc, std::uint32_t* argv_buf_size )
{
#pragma message( "wasi_args_sizes_get can be simplified when entry points are not handled specially" )
  auto args           = _ctx.program_arguments();
  std::uint32_t count = args.size() * 2 - 1;
  std::uint32_t size  = 4; // For entry_point

  for( std::size_t i = 1; i < args.size(); ++i )
  {
    const auto& arg  = args[ i ];
    size            += 4 + arg.size();
  }

  *argc          = count;
  *argv_buf_size = size;

  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t
host_api::wasi_fd_seek( std::uint32_t fd, std::uint64_t offset, std::uint8_t* whence, std::uint8_t* newoffset )
{
  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::wasi_fd_write( std::uint32_t fd, const std::vector< io_vector > iovs, std::uint32_t* nwritten )
{
  if( fd != 1 && fd != 2 )
    return static_cast< std::int32_t >( reversion_errc::failure ); // "can only write to stdout"

  for( auto& iov: iovs )
  {
    _ctx.write( static_cast< program::file_descriptor >( fd ), std::span< const std::byte >( iov.buf, iov.len ) );
    *nwritten += iov.len;
  }

  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::wasi_fd_read( std::uint32_t fd, std::vector< io_vector > iovs, std::uint32_t* nwritten )
{
  *nwritten = 0;

  for( auto& iov: iovs )
  {
    if( auto error =
          _ctx.read( static_cast< program::file_descriptor >( fd ), std::span< std::byte >( iov.buf, iov.len ) );
        error )
      return static_cast< std::int32_t >( error.value() );
    *nwritten += iov.len;
  }

  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::wasi_fd_close( std::uint32_t fd )
{
  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::wasi_fd_fdstat_get( std::uint32_t fd, std::uint32_t* flags )
{
  if( fd == 0 )
  {
    *flags |= 1 << 0; // fd_read
    return 0;
  }
  else if( fd == 1 || fd == 2 )
  {
    *flags |= 1 << 5; // fd_write
    return 0;
  }

  return 8; // badf;
}

void host_api::wasi_proc_exit( std::int32_t exit_code ) {}

std::int32_t host_api::koinos_get_caller( char* ret_ptr, std::uint32_t* ret_len )
{
  auto caller = _ctx.get_caller();
  if( caller.size() > *ret_len )
    return static_cast< std::int32_t >( reversion_errc::failure );

  std::ranges::copy( caller, memory::as_writable_bytes( ret_ptr, *ret_len ).begin() );
  *ret_len = caller.size();

  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::koinos_get_object( std::uint32_t id,
                                          const char* key_ptr,
                                          std::uint32_t key_len,
                                          char* ret_ptr,
                                          std::uint32_t* ret_len )
{
  auto object = _ctx.get_object( id, memory::as_bytes( key_ptr, key_len ) );
  if( object.size() > *ret_len )
    return static_cast< std::int32_t >( reversion_errc::failure );

  std::ranges::copy( object, memory::as_writable_bytes( ret_ptr, *ret_len ).begin() );
  *ret_len = object.size();

  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::koinos_put_object( std::uint32_t id,
                                          const char* key_ptr,
                                          std::uint32_t key_len,
                                          const char* value_ptr,
                                          std::uint32_t value_len )
{
  return static_cast< std::int32_t >(
    _ctx.put_object( id, memory::as_bytes( key_ptr, key_len ), memory::as_bytes( value_ptr, value_len ) ).value() );
}

std::int32_t host_api::koinos_check_authority( const char* account_ptr,
                                               std::uint32_t account_len,
                                               const char* data_ptr,
                                               std::uint32_t data_len,
                                               bool* value )
{
  if( auto authorized = _ctx.check_authority( memory::as_bytes( account_ptr, account_len ) ); authorized )
    *value = *authorized;
  else
    return static_cast< std::int32_t >( authorized.error().value() );

  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::koinos_log( const char* msg_ptr, std::uint32_t msg_len )
{
  _ctx.log( memory::as_bytes( msg_ptr, msg_len ) );
  return 0;
}

} // namespace koinos::controller
