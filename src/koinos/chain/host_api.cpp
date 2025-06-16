
#include <koinos/chain/host_api.hpp>

#include <koinos/memory/memory.hpp>

using namespace std::string_literals;

namespace koinos::chain {

host_api::host_api( execution_context& ctx ):
    _ctx( ctx )
{}

host_api::~host_api() {}

std::int32_t host_api::wasi_args_get( std::uint32_t* argc, std::uint32_t* argv, char* argv_buf )
{
#pragma message( "wasi_args_get can be simplified when entry points are not handled specially" )
  // NOLINTBEGIN
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
  // NOLINTEND
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

std::int32_t host_api::wasi_fd_seek( std::uint32_t fd, uint64_t offset, uint8_t* whence, uint8_t* newoffset )
{
  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t
host_api::wasi_fd_write( std::uint32_t fd, const uint8_t* iovs, std::uint32_t iovs_len, std::uint32_t* nwritten )
{
  if( fd != 1 )
    return static_cast< std::int32_t >( reversion_errc::failure ); // "can only write to stdout"

  _ctx.write_output( std::span< const std::byte >( memory::pointer_cast< const std::byte* >( iovs ), iovs_len ) );
  *nwritten = iovs_len;

  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::wasi_fd_close( std::uint32_t fd )
{
  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::wasi_fd_fdstat_get( std::uint32_t fd, uint8_t* buf_ptr )
{
  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::koinos_get_caller( char* ret_ptr, std::uint32_t* ret_len )
{
  if( auto caller = _ctx.get_caller(); caller )
  {
    if( caller->size() > *ret_len )
      return static_cast< std::int32_t >( reversion_errc::failure );

    std::ranges::copy( *caller, std::as_writable_bytes( std::span( ret_ptr, *ret_len ) ).begin() );
    *ret_len = caller->size();
  }
  else
    return static_cast< std::int32_t >( caller.error().value() );

  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::koinos_get_object( std::uint32_t id,
                                          const char* key_ptr,
                                          std::uint32_t key_len,
                                          char* ret_ptr,
                                          std::uint32_t* ret_len )
{
  if( auto object = _ctx.get_object( id, std::as_bytes( std::span( key_ptr, key_len ) ) ); object )
  {
    if( object->size() > *ret_len )
      return static_cast< std::int32_t >( reversion_errc::failure );

    std::ranges::copy( *object, std::as_writable_bytes( std::span( ret_ptr, *ret_len ) ).begin() );
    *ret_len = object->size();
  }
  else
    return static_cast< std::int32_t >( object.error().value() );

  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::koinos_put_object( std::uint32_t id,
                                          const char* key_ptr,
                                          std::uint32_t key_len,
                                          const char* value_ptr,
                                          std::uint32_t value_len )
{
  return static_cast< std::int32_t >( _ctx
                                        .put_object( id,
                                                     std::as_bytes( std::span( key_ptr, key_len ) ),
                                                     std::as_bytes( std::span( value_ptr, value_len ) ) )
                                        .value() );
}

std::int32_t host_api::koinos_check_authority( const char* account_ptr,
                                               std::uint32_t account_len,
                                               const char* data_ptr,
                                               std::uint32_t data_len,
                                               bool* value )
{
  if( auto authorized = _ctx.check_authority( std::as_bytes( std::span( account_ptr, account_len ) ) ); authorized )
    *value = *authorized;
  else
    return static_cast< std::int32_t >( authorized.error().value() );

  return static_cast< std::int32_t >( reversion_errc::ok );
}

std::int32_t host_api::koinos_log( const char* msg_ptr, std::uint32_t msg_len )
{
  if( auto result = _ctx.log( std::as_bytes( std::span( msg_ptr, msg_len ) ) ); !result )
    return static_cast< std::int32_t >( result.value() );

  return 0;
}

} // namespace koinos::chain
