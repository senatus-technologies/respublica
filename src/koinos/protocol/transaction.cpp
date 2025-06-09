#include <koinos/protocol/transaction.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>
#include <vector>

#include <boost/archive/binary_oarchive.hpp>

#include <koinos/protocol/operation.hpp>

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
#define METHOD0 1

#if METHOD3
  std::vector< std::span< const std::byte > > v;
  v.reserve( 5 );
  v.emplace_back( std::span( t.network_id ) );
  v.emplace_back( std::as_bytes( std::span< const std::uint64_t >( &t.resource_limit, 1 ) ) );
  v.emplace_back( std::span( t.payer ) );
  v.emplace_back( std::span( t.payee ) );
  v.emplace_back( std::as_bytes( std::span< const std::uint64_t >( &t.nonce, 1 ) ) );
  v.emplace_back( std::as_bytes( std::span( t.operations ) ) );

  return crypto::merkle_root( v );
#endif

#if METHOD2
  std::stringstream ss;
  boost::archive::binary_oarchive oa( ss, boost::archive::no_tracking );

  oa << t.network_id;
  oa << t.resource_limit;
  oa << t.payer;
  oa << t.payee;
  oa << t.nonce;

  for( const auto& operation: t.operations )
    oa << operation;

  return crypto::hash( static_cast< const void* >( ss.view().data() ), ss.view().size() );
#endif

#if METHOD1
  crypto::hasher_reset();

  crypto::hasher_update( &t.network_id, sizeof( t.network_id ) );
  crypto::hasher_update( &t.resource_limit, sizeof( t.resource_limit ) );
  crypto::hasher_update( &t.payer, sizeof( t.payer ) );
  crypto::hasher_update( &t.payee, sizeof( t.payee ) );
  crypto::hasher_update( &t.nonce, sizeof( t.nonce ) );

  for( const auto& operation: t.operations )
  {
    if( std::holds_alternative< upload_program >( operation ) )
    {
      const auto& upload = std::get< upload_program >( operation );
      crypto::hasher_update( &upload.id, sizeof( upload.id ) );
      crypto::hasher_update( upload.bytecode.data(), upload.bytecode.size() );
    }
    else if( std::holds_alternative< call_program >( operation ) )
    {
      const auto& call = std::get< call_program >( operation );
      crypto::hasher_update( &call.id, sizeof( call.id ) );
      crypto::hasher_update( &call.entry_point, sizeof( call.entry_point ) );
      for( const auto& argument: call.arguments )
        crypto::hasher_update( argument.data(), argument.size() );
    }
  }

  return crypto::hasher_finalize();
#endif

#if METHOD0
  crypto::hasher_reset();
  crypto::hasher_update( t.network_id );
  crypto::hasher_update( t.resource_limit );

  crypto::hasher_update( t.payer );
  crypto::hasher_update( t.payee );
  crypto::hasher_update( t.nonce );

  for( const auto& operation: t.operations )
  {
    if( std::holds_alternative< upload_program >( operation ) )
    {
      const auto& upload = std::get< upload_program >( operation );
      crypto::hasher_update( upload.id );
      crypto::hasher_update( upload.bytecode );
    }
    else if( std::holds_alternative< call_program >( operation ) )
    {
      const auto& call = std::get< call_program >( operation );
      crypto::hasher_update( call.id );
      crypto::hasher_update( call.entry_point );
      crypto::hasher_update( call.arguments );
    }
  }
  return crypto::hasher_finalize();
#endif
}

} // namespace koinos::protocol
