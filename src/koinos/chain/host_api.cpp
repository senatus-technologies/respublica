
#include <boost/locale/utf.hpp>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/system_call_ids.pb.h>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>

#include <koinos/log.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <koinos/util/hex.hpp>
#include <koinos/vm_manager/timer.hpp>

using namespace std::string_literals;

namespace koinos::chain {

host_api::host_api( execution_context& ctx ):
    _ctx( ctx )
{}

host_api::~host_api() {}

constexpr uint32_t mint_entry     = 0xdc6f17bb;
constexpr uint32_t transfer_entry = 0x27f576ca;
const std::string contract_address = util::from_base58< std::string >( "1926y6iq9HE7XG81oMroEB3iAz7UFpTiE2" );
const std::string alice_address = util::from_base58< std::string >( "15iVSHUXH52WWbEdoZNpkGXXqZUM91uu6W" );

uint32_t host_api::wasi_args_get(uint32_t* argc, uint32_t* argv, char* argv_buf )
{
  KOINOS_TIMER( "host_api::wasi_args_get" );
  return with_stack_frame(
    _ctx,
    stack_frame{},
    [ & ]{
      auto entry_point = _ctx.get_contract_entry_point();
      uint32_t counter = 0;

      // Mint case
      if( entry_point == mint_entry )
      {
        argv[0] = counter;
        memcpy( argv_buf + counter, &mint_entry, sizeof(mint_entry) );
        counter += sizeof(mint_entry);

        argv[1] = counter;
        uint32_t size = alice_address.size();
        memcpy( argv_buf + counter, &size, sizeof(uint32_t) );
        counter += sizeof(uint32_t);

        argv[2] = counter;
        memcpy( argv_buf + counter, alice_address.data(), alice_address.size() );
        counter += alice_address.size();

        argv[3] = counter;
        size = sizeof(uint64_t);
        memcpy( argv_buf + counter, &size, sizeof(uint32_t) );
        counter += sizeof(uint32_t);

        argv[4] = counter;
        uint64_t value = 100;
        memcpy( argv_buf + counter, &value, sizeof(uint64_t) );
        counter += sizeof(uint64_t);

        *argc = 5;
      }
      // Transfer case
      else if( entry_point == transfer_entry )
      {
        argv[0] = counter;
        memcpy( argv_buf + counter, &transfer_entry, sizeof(transfer_entry) );
        counter += sizeof(transfer_entry);

        argv[1] = counter;
        uint32_t size = alice_address.size();
        memcpy( argv_buf + counter, &size, sizeof(uint32_t) );
        counter += sizeof(uint32_t);

        argv[2] = counter;
        memcpy( argv_buf + counter, alice_address.data(), alice_address.size() );
        counter += alice_address.size();

        argv[3] = counter;
        size = contract_address.size();
        memcpy( argv_buf + counter, &size, sizeof(uint32_t) );
        counter += sizeof(uint32_t);

        argv[4] = counter;
        memcpy( argv_buf + counter, contract_address.data(), contract_address.size() );
        counter += contract_address.size();

        argv[5] = counter;
        size = sizeof(uint64_t);
        memcpy( argv_buf + counter, &size, sizeof(uint32_t) );
        counter += sizeof(uint32_t);

        argv[6] = counter;
        uint64_t value = 100;
        memcpy( argv_buf + counter, &value, sizeof(uint64_t) );
        counter += sizeof(uint64_t);

        *argc = 7;
      }

      return success;
  });
}

uint32_t host_api::wasi_args_sizes_get( uint32_t* argc, uint32_t* argv_buf_size )
{
  KOINOS_TIMER( "host_api::wasi_args_sizes_get" );
  return with_stack_frame(
    _ctx,
    stack_frame{},
    [ & ]{
      auto entry_point = _ctx.get_contract_entry_point();

      // Mint case
      if( entry_point == mint_entry )
      {
        *argc = 5;
        *argv_buf_size = 45;
      }
      // Transfer case
      else if( entry_point == transfer_entry )
      {
        *argc = 7;
        *argv_buf_size = 74;
      }

      return success;
  });
}

uint32_t host_api::wasi_fd_seek( uint32_t fd, uint64_t offset, uint8_t* whence, uint8_t* newoffset )
{
  return 0;
}

uint32_t host_api::wasi_fd_write( uint32_t fd, const uint8_t* iovs, uint32_t iovs_len, uint32_t* nwritten )
{
  KOINOS_TIMER( "host_api::wasi_fd_write" );
  return with_stack_frame(
    _ctx,
    stack_frame{},
    [ & ]{

      KOINOS_ASSERT( fd == 1, reversion_exception, "can only write to stdout" );

      _ctx.write_output( std::span< const std::byte >( reinterpret_cast< const std::byte* >( iovs ), iovs_len ) );
      *nwritten = iovs_len;

      return success;
  });
}

uint32_t host_api::wasi_fd_close( uint32_t fd )
{
  return 0;
}

uint32_t host_api::wasi_fd_fdstat_get( uint32_t fd, uint8_t* buf_ptr )
{
  return 0;
}

int32_t host_api::koinos_get_caller( char* ret_ptr, uint32_t* ret_len )
{
  KOINOS_TIMER( "host_api::koinos_get_caller" );
  return with_stack_frame(
    _ctx,
    stack_frame{},
    [ & ]{
      if( !ret_ptr || !ret_len )
        return chain::reversion;

      auto frame0 = _ctx.pop_frame();
      auto frame1 = _ctx.pop_frame();

      const auto& caller = _ctx.get_caller();
      if( caller.size() > *ret_len )
      {
        _ctx.push_frame( std::move( frame1 ) );
        _ctx.push_frame( std::move( frame0 ) );
        return chain::reversion;
      }

      memcpy( ret_ptr, caller.data(), caller.size() );
      *ret_len = caller.size();

      _ctx.push_frame( std::move( frame1 ) );
      _ctx.push_frame( std::move( frame0 ) );

      return chain::success;
  });
}

int32_t host_api::koinos_get_object( uint32_t id, const char* key_ptr, uint32_t key_len, char* ret_ptr, uint32_t* ret_len )
{
  KOINOS_TIMER( "host_api::koinos_get_object" );
  return with_stack_frame(
    _ctx,
    stack_frame{},
    [ & ]{
      if( !key_ptr || !ret_ptr || !ret_len )
        return chain::reversion;

      auto state = _ctx.get_state_node();
      if( !state )
        return chain::internal_error;

      object_space space;
      space.set_system( false );
      space.set_zone( _ctx.get_caller() );
      space.set_id( id );

      const auto result = state->get_object( space, std::string( key_ptr, key_len ) );
      if( result )
      {
        memcpy( ret_ptr, result->c_str(), result->size() );
        *ret_len = result->size();
      }
      else
        *ret_len = 0;

      return chain::success;
  });
}

int32_t host_api::koinos_put_object( uint32_t id, const char* key_ptr, uint32_t key_len, const char* value_ptr, uint32_t value_len )
{
  KOINOS_TIMER( "host_api::koinos_put_object" );
  return with_stack_frame(
    _ctx,
    stack_frame{},
    [ & ]{
      if( !key_ptr || !key_ptr )
        return chain::reversion;

      auto state = _ctx.get_state_node();
      if( !state )
        return chain::internal_error;

      object_space space;
      space.set_system( false );
      space.set_zone( _ctx.get_caller() );
      space.set_id( id );

      state_db::object_value value( value_ptr, value_len );

      state->put_object( space, std::string( key_ptr, key_len), &value );

      return chain::success;
  });
}

int32_t host_api::koinos_check_authority( const char* account_ptr, uint32_t account_len, const char* data_ptr, uint32_t data_len, bool* value )
{
  KOINOS_TIMER( "host_api::koinos_check_authority" );
  return with_stack_frame(
    _ctx,
    stack_frame{},
    [ & ]{
      if( !account_ptr || !data_ptr )
        return chain::reversion;

      const auto* trx = _ctx.get_transaction();
      if( !trx )
        return chain::internal_error;

      bool authorized = false;
      std::string_view account( account_ptr, account_len );

      for( const auto& sig: trx->signatures() )
      {
        auto signer_address = util::converter::to< crypto::public_key >(
          system_call::recover_public_key( _ctx, ecdsa_secp256k1, sig, trx->id(), true ) )
          .to_address_bytes();

        authorized = ( signer_address == account );
        if( authorized )
          break;
      }

      *value = authorized;

      return chain::success;
  });
}

template< typename T >
bool validate_utf( const std::basic_string< T >& p_str )
{
  typename std::basic_string< T >::const_iterator it = p_str.begin();
  while( it != p_str.end() )
  {
    const boost::locale::utf::code_point cp = boost::locale::utf::utf_traits< T >::decode( it, p_str.end() );
    if( cp == boost::locale::utf::illegal )
      return false;
    else if( cp == boost::locale::utf::incomplete )
      return false;
  }
  return true;
}

int32_t host_api::koinos_log( const char* msg_ptr, uint32_t msg_len )
{
  KOINOS_TIMER( "host_api::koinos_log" );
  return with_stack_frame(
    _ctx,
    stack_frame{},
    [ & ]{
      if( !msg_ptr )
        return chain::reversion;
      auto msg = std::string( msg_ptr, msg_len );

      if( !validate_utf( msg ) )
        return chain::reversion;

      _ctx.chronicler().push_log( msg );
      LOG(info) << std::string( msg_ptr, msg_len );

      return chain::success;
  });
}

int32_t host_api::koinos_exit( int32_t code, const char* res_bytes, uint32_t res_len )
{
  KOINOS_TIMER( "host_api::koinos_exit" );
  with_stack_frame(
    _ctx,
    stack_frame{},
    [ & ]{
      result res;
      std::string res_str;

      if( res_bytes )
      {
        res_str = std::string( res_bytes, res_len );

        if( code == success )
          res.set_object( res_str );
        else
        {
          res.mutable_error()->set_message( res_str );
          LOG(info) << res_str;
        }
      }

      _ctx.set_result( {code, res } );

      if( code >= reversion )
        BOOST_THROW_EXCEPTION( reversion_exception( code, res_str ) );
      if( code <= failure )
        BOOST_THROW_EXCEPTION( failure_exception( code, res_str ) );

      throw success_exception();
  });

  return chain::success;
}

int32_t host_api::koinos_get_arguments( uint32_t* entry_point, char* args_ptr, uint32_t* args_len )
{
  return with_stack_frame(
    _ctx,
    stack_frame{},
    [ & ]{
      if( !entry_point || !args_ptr || !args_len )
        return chain::reversion;

      *entry_point = _ctx.get_contract_entry_point();
      const auto& args = _ctx.get_contract_call_args();

      memcpy( args_ptr, args.c_str(), args.size() );

      return chain::success;
  });
}

int64_t host_api::get_meter_ticks() const
{
  auto compute_bandwidth_remaining = _ctx.resource_meter().compute_bandwidth_remaining();

  // If we have more ticks than fizzy can accept
  if( compute_bandwidth_remaining > std::numeric_limits< int64_t >::max() )
    compute_bandwidth_remaining = std::numeric_limits< int64_t >::max();

  int64_t ticks = compute_bandwidth_remaining;
  return ticks;
}

void host_api::use_meter_ticks( uint64_t meter_ticks )
{
  if( meter_ticks > _ctx.resource_meter().compute_bandwidth_remaining() )
  {
    _ctx.resource_meter().use_compute_bandwidth( _ctx.resource_meter().compute_bandwidth_remaining() );
    _ctx.resource_meter().use_compute_bandwidth( 1 );
  }
  else
  {
    _ctx.resource_meter().use_compute_bandwidth( meter_ticks );
  }
}

} // namespace koinos::chain
