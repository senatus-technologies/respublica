
#include <boost/locale/utf.hpp>

#include <koinos/chain/constants.hpp>
#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/system_call_ids.pb.h>
#include <koinos/chain/system_calls.hpp>
#include <koinos/chain/thunk_dispatcher.hpp>

#include <koinos/log.hpp>
#include <koinos/util/conversion.hpp>

using namespace std::string_literals;

namespace koinos::chain {

host_api::host_api( execution_context& ctx ):
    _ctx( ctx )
{}

host_api::~host_api() {}

int32_t host_api::invoke_thunk( uint32_t tid,
                                char* ret_ptr,
                                uint32_t ret_len,
                                const char* arg_ptr,
                                uint32_t arg_len,
                                uint32_t* bytes_written )
{
  KOINOS_ASSERT( _ctx.get_privilege() == privilege::kernel_mode,
                 insufficient_privileges_exception,
                 "'invoke_thunk' must be called from a system context" );

  int32_t code = 0;
  error_data error;

  try
  {
    thunk_dispatcher::instance().call_thunk( tid, _ctx, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
  }
  catch( koinos::exception& e )
  {
    code  = e.get_code();
    error = e.get_data();
  }

  if( tid == system_call_id::exit )
  {
    if( code >= chain::reversion )
      throw reversion_exception( code, error );
    if( code <= chain::failure )
      throw failure_exception( code, error );

    throw success_exception( code );
  }

  if( code != chain::success )
  {
    auto error_bytes = util::converter::as< std::string >( error );

    auto msg_len = error_bytes.size();
    if( msg_len <= ret_len )
    {
      std::memcpy( ret_ptr, error_bytes.data(), msg_len );
      *bytes_written = uint32_t( msg_len );
    }
    else
    {
      code           = chain::insufficient_return_buffer;
      *bytes_written = 0;
    }
  }

  return code;
}

void host_api::call( uint32_t sid,
                     char* ret_ptr,
                     uint32_t ret_len,
                     const char* arg_ptr,
                     uint32_t arg_len,
                     uint32_t* bytes_written )
{
  *bytes_written = 0;

  with_stack_frame(
    _ctx,
    stack_frame{ .sid = sid, .call_privilege = privilege::kernel_mode },
    [ & ]()
    {
      if( _ctx.system_call_exists( sid ) )
      {
        std::string args( arg_ptr, arg_len );
        auto exec_res = _ctx.system_call( sid, args );

        if( exec_res.res.has_object() )
        {
          auto obj_len = exec_res.res.object().size();
          KOINOS_ASSERT( obj_len <= ret_len,
                         insufficient_return_buffer_exception,
                         "return buffer is not large enough for the return value" );
          memcpy( ret_ptr, exec_res.res.object().data(), obj_len );
          *bytes_written = uint32_t( obj_len );
        }
      }
      else
      {
        auto thunk_id = _ctx.thunk_translation( sid );
        KOINOS_ASSERT( thunk_dispatcher::instance().thunk_exists( thunk_id ),
                       unknown_thunk_exception,
                       "thunk ${tid} does not exist",
                       ( "tid", thunk_id ) );
        auto desc       = chain::system_call_id_descriptor();
        auto enum_value = desc->FindValueByNumber( thunk_id );
        KOINOS_ASSERT( enum_value, unknown_thunk_exception, "unrecognized thunk id ${id}", ( "id", thunk_id ) );
        auto compute = _ctx.get_compute_bandwidth( enum_value->name() );
        _ctx.resource_meter().use_compute_bandwidth( compute );
        thunk_dispatcher::instance().call_thunk( thunk_id, _ctx, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
      }
    } );
}

int32_t host_api::invoke_system_call( uint32_t sid,
                                      char* ret_ptr,
                                      uint32_t ret_len,
                                      const char* arg_ptr,
                                      uint32_t arg_len,
                                      uint32_t* bytes_written )
{
  int32_t code = 0;
  error_data error;

  try
  {
    call( sid, ret_ptr, ret_len, arg_ptr, arg_len, bytes_written );
  }
  catch( const koinos::exception& e )
  {
    error = e.get_data();
    code  = e.get_code();
  }

  if( _ctx.get_privilege() == privilege::user_mode && code >= reversion )
    throw reversion_exception( code, error );

  if( sid == system_call_id::exit )
  {
    if( code >= reversion )
      throw reversion_exception( code, error );
    if( code <= failure )
      throw failure_exception( code, error );

    throw success_exception();
  }

  if( code != chain::success )
  {
    auto error_bytes = util::converter::as< std::string >( error );

    auto msg_len = error_bytes.size();
    if( msg_len <= ret_len )
    {
      std::memcpy( ret_ptr, error_bytes.data(), msg_len );
      *bytes_written = uint32_t( msg_len );
    }
    else
    {
      code           = chain::insufficient_return_buffer;
      *bytes_written = 0;
    }
  }

  return code;
}

int32_t host_api::koinos_get_caller( char* ret_ptr, uint32_t* ret_len )
{
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

      const auto result = state->get_object( space, std::string( ret_ptr, *ret_len ) );
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

      return chain::success;
  });
}

int32_t host_api::koinos_exit( int32_t code, const char* res_bytes, uint32_t res_len )
{
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
          res.mutable_error()->set_message( res_str );
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
