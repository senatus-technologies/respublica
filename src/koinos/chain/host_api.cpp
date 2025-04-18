
#include <boost/locale/utf.hpp>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>

#include <koinos/log.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>
#include <koinos/vm_manager/timer.hpp>

using namespace std::string_literals;

namespace koinos::chain {

using koinos::error::error_code;

host_api::host_api( execution_context& ctx ):
    _ctx( ctx )
{}

host_api::~host_api() {}

constexpr uint32_t mint_entry     = 0xdc6f17bb;
constexpr uint32_t transfer_entry = 0x27f576ca;
const std::string contract_address = util::from_base58< std::string >( "1926y6iq9HE7XG81oMroEB3iAz7UFpTiE2" );
const std::string alice_address    = util::from_base58< std::string >( "15iVSHUXH52WWbEdoZNpkGXXqZUM91uu6W" );

int32_t host_api::wasi_args_get( uint32_t* argc, uint32_t* argv, char* argv_buf )
{
  KOINOS_TIMER( "host_api::wasi_args_get" );

  auto args = _ctx.contract_arguments();
  if( !args )
    return static_cast< int32_t >( args.error().value() );

  auto entry_point = _ctx.contract_entry_point();
  if( !entry_point )
    return static_cast< int32_t >( entry_point.error().value() );

  uint32_t counter = 0;
  uint32_t index = 0;

  argv[ index++ ] = counter;
  memcpy( argv_buf + counter, &*entry_point, sizeof( *entry_point ) );
  counter += sizeof( *entry_point );

  for( const auto& arg : *args )
  {
    argv[ index++ ] = counter;
    uint32_t size = arg.size();
    memcpy( argv_buf + counter, &size, sizeof( uint32_t ) );
    counter += sizeof( uint32_t );

    argv[ index++ ] = counter;
    memcpy( argv_buf + counter, arg.data(), arg.size() );
    counter += sizeof( arg.size() );
  }

  return static_cast< int32_t >( error_code::success );
}

int32_t host_api::wasi_args_sizes_get( uint32_t* argc, uint32_t* argv_buf_size )
{
  KOINOS_TIMER( "host_api::wasi_args_sizes_get" );

  if( auto args = _ctx.contract_arguments(); args )
  {
    uint32_t count = args->size() * 2 + 1;
    uint32_t size = 4; // For entry_point

    for( const auto& arg : *args )
      size += 4 + arg.size();

    *argc = count;
    *argv_buf_size = size;
  }
  else
    return static_cast< int32_t >( args.error().value() );

  return static_cast< int32_t >( error_code::success );
}

int32_t host_api::wasi_fd_seek( uint32_t fd, uint64_t offset, uint8_t* whence, uint8_t* newoffset )
{
  return static_cast< int32_t >( error_code::success );
}

int32_t host_api::wasi_fd_write( uint32_t fd, const uint8_t* iovs, uint32_t iovs_len, uint32_t* nwritten )
{
  KOINOS_TIMER( "host_api::wasi_fd_write" );
  if( fd != 1 )
  return static_cast< int32_t >( error_code::reversion ); // "can only write to stdout"

  _ctx.write_output(
    bytes_s( reinterpret_cast< const std::byte* >( iovs ), iovs_len ) );
  *nwritten = iovs_len;

  return static_cast< int32_t >( error_code::success );
}

int32_t host_api::wasi_fd_close( uint32_t fd )
{
  return static_cast< int32_t >( error_code::success );
}

int32_t host_api::wasi_fd_fdstat_get( uint32_t fd, uint8_t* buf_ptr )
{
  return static_cast< int32_t >( error_code::success );
}

int32_t host_api::koinos_get_caller( char* ret_ptr, uint32_t* ret_len )
{
  KOINOS_TIMER( "host_api::koinos_get_caller" );
  if( auto caller = _ctx.get_caller(); caller )
  {
    if( caller->size() > *ret_len )
      return static_cast< int32_t >( error_code::reversion );

    std::span< std::byte > ret_span( reinterpret_cast< std::byte* >( ret_ptr ), reinterpret_cast< std::byte* >( ret_ptr ) + *ret_len );
    std::copy( caller->begin(), caller->end(), ret_span.begin() );
    *ret_len = caller->size();
  }
  else
    return static_cast< int32_t >( caller.error().value() );

  return static_cast< int32_t >( error_code::success );
}

int32_t
host_api::koinos_get_object( uint32_t id, const char* key_ptr, uint32_t key_len, char* ret_ptr, uint32_t* ret_len )
{
  KOINOS_TIMER( "host_api::koinos_get_object" );
  if( auto object = _ctx.get_object( id, bytes_s( reinterpret_cast< const std::byte* >( key_ptr ), reinterpret_cast< const std::byte* >( key_ptr ) + key_len ) ); object )
  {
    if( object->size() > *ret_len )
      return static_cast< int32_t >( error_code::reversion );

    std::span< std::byte > ret_span( reinterpret_cast< std::byte* >( ret_ptr ), reinterpret_cast< std::byte* >( ret_ptr ) + *ret_len );
    std::copy( object->begin(), object->end(), ret_span.begin() );
    *ret_len = object->size();
  }
  else
    return static_cast< int32_t >( object.error().value() );

  return static_cast< int32_t >( error_code::success );
}

int32_t host_api::koinos_put_object( uint32_t id,
                                     const char* key_ptr,
                                     uint32_t key_len,
                                     const char* value_ptr,
                                     uint32_t value_len )
{
  KOINOS_TIMER( "host_api::koinos_put_object" );
  return static_cast< int32_t >( _ctx.put_object(
    id,
    bytes_s( reinterpret_cast< const std::byte* >( key_ptr ), reinterpret_cast< const std::byte* >( key_ptr ) + key_len ),
    bytes_s( reinterpret_cast< const std::byte* >( value_ptr ), reinterpret_cast< const std::byte* >( value_ptr ) + value_len ) ).value() );
}

int32_t host_api::koinos_check_authority( const char* account_ptr,
                                          uint32_t account_len,
                                          const char* data_ptr,
                                          uint32_t data_len,
                                          bool* value )
{
  KOINOS_TIMER( "host_api::koinos_check_authority" );
  if( auto authorized = _ctx.check_authority( bytes_s( reinterpret_cast< const std::byte* >( account_ptr ), reinterpret_cast< const std::byte* >( account_ptr ) + account_len ) ); authorized )
    *value = *authorized;
  else
    return static_cast< int32_t >( authorized.error().value() );

  return static_cast< int32_t >( error_code::success );
}

int32_t host_api::koinos_log( const char* msg_ptr, uint32_t msg_len )
{
  KOINOS_TIMER( "host_api::koinos_log" );
  if( auto result = _ctx.log( bytes_s( reinterpret_cast< const std::byte* >( msg_ptr ), reinterpret_cast< const std::byte* >( msg_ptr ) + msg_len ) ); !result )
    return static_cast< int32_t >( result.error().value() );

  return 0;
}

} // namespace koinos::chain
