
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
  const auto arguments  = _ctx.arguments();
  std::uint32_t counter = 0;
  std::uint32_t index   = 0;

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  for( std::size_t i = 0; i < arguments.size(); ++i )
  {
    argv[ index ] = counter;
    std::memcpy( argv_buf + counter, arguments[ i ].data(), arguments[ i ].size() );
    counter += arguments[ i ].size();
  }
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  *argc = index;

  return static_cast< std::int32_t >( vm::wasi_errno::success );
}

std::int32_t host_api::wasi_args_sizes_get( std::uint32_t* argc, std::uint32_t* argv_buf_size )
{
  const auto arguments = _ctx.arguments();

  *argc          = arguments.size();
  *argv_buf_size = 0;
  for( const auto& argument: arguments )
    *argv_buf_size += argument.size();

  return static_cast< std::int32_t >( vm::wasi_errno::success );
}

std::int32_t
host_api::wasi_fd_seek( std::uint32_t fd, std::uint64_t offset, std::uint8_t* whence, std::uint8_t* newoffset )
{
#pragma message( "TODO: Implement wasi_fd_seek()" )
  return static_cast< std::int32_t >( vm::wasi_errno::success );
}

std::int32_t
host_api::wasi_fd_write( std::uint32_t fd, const std::vector< vm::io_vector > iovs, std::uint32_t* nwritten )
{
  for( auto& iov: iovs )
  {
    if( auto error = _ctx.write( static_cast< program::file_descriptor >( fd ), iov ); error )
      return static_cast< std::int32_t >( vm::wasi_errno::badf );
    *nwritten += iov.size();
  }

  return static_cast< std::int32_t >( vm::wasi_errno::success );
}

std::int32_t host_api::wasi_fd_read( std::uint32_t fd, std::vector< vm::io_vector > iovs, std::uint32_t* nwritten )
{
  *nwritten = 0;

  for( auto& iov: iovs )
  {
    if( auto error = _ctx.read( static_cast< program::file_descriptor >( fd ), iov ); error )
      return static_cast< std::int32_t >( vm::wasi_errno::badf );
    *nwritten += iov.size();
  }

  return static_cast< std::int32_t >( vm::wasi_errno::success );
}

std::int32_t host_api::wasi_fd_close( std::uint32_t fd )
{
#pragma message( "TODO: Implement wasi_fd_close()" )
  return static_cast< std::int32_t >( vm::wasi_errno::success );
}

std::int32_t host_api::wasi_fd_fdstat_get( std::uint32_t fd, std::uint32_t* flags )
{
  if( fd == std::to_underlying( vm::wasi_fd::stdin ) )
  {
    *flags |= std::to_underlying( vm::wasi_fd_rights::fd_read );
    return static_cast< std::int32_t >( vm::wasi_errno::success );
  }
  else if( fd == std::to_underlying( vm::wasi_fd::stdout ) || fd == std::to_underlying( vm::wasi_fd::stderr ) )
  {
    *flags |= std::to_underlying( vm::wasi_fd_rights::fd_write );
    return static_cast< std::int32_t >( vm::wasi_errno::success );
  }

  return static_cast< std::int32_t >( vm::wasi_errno::badf );
}

void host_api::wasi_proc_exit( std::int32_t exit_code ) {}

std::int32_t host_api::koinos_get_caller( char* ret_ptr, std::uint32_t* ret_len )
{
  auto caller = _ctx.get_caller();
  if( caller.size() > *ret_len )
    return static_cast< std::int32_t >( controller_errc::insufficient_space );

  std::ranges::copy( caller, memory::as_writable_bytes( ret_ptr, *ret_len ).begin() );
  *ret_len = caller.size();

  return static_cast< std::int32_t >( controller_errc::ok );
}

std::int32_t host_api::koinos_get_object( std::uint32_t id,
                                          const char* key_ptr,
                                          std::uint32_t key_len,
                                          char* ret_ptr,
                                          std::uint32_t* ret_len )
{
  auto object = _ctx.get_object( id, memory::as_bytes( key_ptr, key_len ) );
  if( object.size() > *ret_len )
    return static_cast< std::int32_t >( controller_errc::insufficient_space );

  std::ranges::copy( object, memory::as_writable_bytes( ret_ptr, *ret_len ).begin() );
  *ret_len = object.size();

  return static_cast< std::int32_t >( controller_errc::ok );
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

std::int32_t host_api::koinos_check_authority( const char* account_ptr, std::uint32_t account_len, bool* value )
{
  if( account_len != sizeof( protocol::account ) )
    return static_cast< std::int32_t >( controller_errc::invalid_account );

  if( auto authorized = _ctx.check_authority(
        protocol::account_view( memory::pointer_cast< const std::byte* >( account_ptr ), account_len ) );
      authorized )
    *value = *authorized;
  else
    return static_cast< std::int32_t >( authorized.error().value() );

  return static_cast< std::int32_t >( controller_errc::ok );
}

} // namespace koinos::controller
