
#include <koinos/controller/host_api.hpp>

#include <koinos/memory.hpp>
#include <utility>

using namespace std::string_literals;

namespace koinos::controller {

host_api::host_api( execution_context& ctx ):
    _ctx( ctx )
{}

host_api::~host_api() {}

std::error_code host_api::wasi_args_get( std::uint32_t* argc, std::uint32_t* argv, char* argv_buf )
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

  return vm::wasi_errc::success;
}

std::error_code host_api::wasi_args_sizes_get( std::uint32_t* argc, std::uint32_t* argv_buf_size )
{
  const auto arguments = _ctx.arguments();

  *argc          = arguments.size();
  *argv_buf_size = 0;
  for( const auto& argument: arguments )
    *argv_buf_size += argument.size();

  return vm::wasi_errc::success;
}

std::error_code
host_api::wasi_fd_seek( std::uint32_t fd, std::uint64_t offset, std::uint8_t* whence, std::uint8_t* newoffset )
{
#pragma message( "TODO: Implement wasi_fd_seek()" )
  return vm::wasi_errc::success;
}

std::error_code
host_api::wasi_fd_write( std::uint32_t fd, const std::vector< vm::io_vector > iovs, std::uint32_t* nwritten )
{
  for( auto& iov: iovs )
  {
    if( auto error = _ctx.write( static_cast< program::file_descriptor >( fd ), iov ); error )
      return vm::wasi_errc::badf;
    *nwritten += iov.size();
  }

  return vm::wasi_errc::success;
}

std::error_code host_api::wasi_fd_read( std::uint32_t fd, std::vector< vm::io_vector > iovs, std::uint32_t* nwritten )
{
  *nwritten = 0;

  for( auto& iov: iovs )
  {
    if( auto error = _ctx.read( static_cast< program::file_descriptor >( fd ), iov ); error )
      return vm::wasi_errc::badf;
    *nwritten += iov.size();
  }

  return vm::wasi_errc::success;
}

std::error_code host_api::wasi_fd_close( std::uint32_t fd )
{
#pragma message( "TODO: Implement wasi_fd_close()" )
  return vm::wasi_errc::success;
}

std::error_code host_api::wasi_fd_fdstat_get( std::uint32_t fd, std::uint32_t* flags )
{
  if( fd == std::to_underlying( vm::wasi_fd::stdin ) )
  {
    *flags |= std::to_underlying( vm::wasi_fd_rights::fd_read );
    return vm::wasi_errc::success;
  }
  else if( fd == std::to_underlying( vm::wasi_fd::stdout ) || fd == std::to_underlying( vm::wasi_fd::stderr ) )
  {
    *flags |= std::to_underlying( vm::wasi_fd_rights::fd_write );
    return vm::wasi_errc::success;
  }

  return vm::wasi_errc::badf;
}

void host_api::wasi_proc_exit( std::int32_t exit_code ) {}

std::error_code host_api::koinos_get_caller( char* ret_ptr, std::uint32_t* ret_len )
{
  auto caller = _ctx.get_caller();
  if( caller.size() > *ret_len )
    return controller_errc::insufficient_space;

  std::ranges::copy( caller, memory::as_writable_bytes( ret_ptr, *ret_len ).begin() );
  *ret_len = caller.size();

  return controller_errc::ok;
}

std::error_code host_api::koinos_get_object( std::uint32_t id,
                                             const char* key_ptr,
                                             std::uint32_t key_len,
                                             char* ret_ptr,
                                             std::uint32_t* ret_len )
{
  auto object = _ctx.get_object( id, memory::as_bytes( key_ptr, key_len ) );
  if( object.size() > *ret_len )
    return controller_errc::insufficient_space;

  std::ranges::copy( object, memory::as_writable_bytes( ret_ptr, *ret_len ).begin() );
  *ret_len = object.size();

  return controller_errc::ok;
}

std::error_code host_api::koinos_put_object( std::uint32_t id,
                                             const char* key_ptr,
                                             std::uint32_t key_len,
                                             const char* value_ptr,
                                             std::uint32_t value_len )
{
  return _ctx.put_object( id, memory::as_bytes( key_ptr, key_len ), memory::as_bytes( value_ptr, value_len ) );
}

std::error_code host_api::koinos_check_authority( const char* account_ptr, std::uint32_t account_len, bool* value )
{
  if( account_len != sizeof( protocol::account ) )
    return controller_errc::invalid_account;

  if( auto authorized = _ctx.check_authority(
        protocol::account_view( memory::pointer_cast< const std::byte* >( account_ptr ), account_len ) );
      authorized )
    *value = *authorized;
  else
    return authorized.error();

  return controller_errc::ok;
}

std::uint64_t host_api::get_meter_ticks()
{
  return _ctx.get_meter_ticks();
}

std::error_code host_api::use_meter_ticks( std::uint64_t meter_ticks )
{
  return _ctx.use_meter_ticks( meter_ticks );
}

bool host_api::halts( const std::error_code& e ) const noexcept
{
  if( e.category() != controller_category() )
    return false;

  switch( static_cast< controller_errc >( e.value() ) )
  {
    case controller_errc::insufficient_resources:
    case controller_errc::network_bandwidth_limit_exceeded:
    case controller_errc::compute_bandwidth_limit_exceeded:
    case controller_errc::disk_storage_limit_exceeded:
      return true;
    default:
  }

  return false;
}

} // namespace koinos::controller
