#include <koinos/protocol/transaction.hpp>

#include <sstream>

#include <boost/archive/binary_oarchive.hpp>

namespace koinos::protocol {

std::size_t transaction::size() const noexcept
{
  std::size_t bytes = 0;

  bytes += id.size();
  bytes += network_id.size();
  bytes += sizeof( resource_limit );
  bytes += payer.size();
  bytes += payee.size();
  bytes += sizeof( nonce );

  for( const auto& operation: operations )
  {
    if( std::holds_alternative< upload_program >( operation ) )
      bytes += std::get< upload_program >( operation ).size();
    else if( std::holds_alternative< call_program >( operation ) )
      bytes += std::get< call_program >( operation ).size();
  }

  bytes += authorizations.size();

  return bytes;
}

bool transaction::validate() const noexcept
{
  if( !resource_limit )
    return false;

  if( make_id( *this ) != id )
    return false;

  return true;
}

crypto::digest make_id( const transaction& t ) noexcept
{
  std::stringstream ss;
  boost::archive::binary_oarchive oa( ss, boost::archive::no_tracking );

  oa << t.network_id;
  oa << t.resource_limit;
  oa << t.payer;
  oa << t.payee;
  oa << t.nonce;

  for( const auto& operation: t.operations )
    oa << operation;

  for( const auto& authorization: t.authorizations )
    oa << authorization.signer;

  return crypto::hash( static_cast< const void* >( ss.view().data() ), ss.view().size() );
#if 0
  crypto::hasher_reset();

  crypto::hasher_update( t.network_id );
  crypto::hasher_update( t.resource_limit );
  crypto::hasher_update( t.payer );
  crypto::hasher_update( t.payee );
  crypto::hasher_update( t.nonce );

  for( const auto& operation: t.operations )
    crypto::hasher_update( operation );

  for( const auto& authorization: t.authorizations )
    crypto::hasher_update( authorization.signer );

  return crypto::hasher_finalize();
#endif
}

} // namespace koinos::protocol
