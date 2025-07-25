#include <respublica/protocol/transaction.hpp>

#include <cstddef>
#include <variant>
#include <vector>

#include <boost/archive/binary_oarchive.hpp>

#include <respublica/protocol/operation.hpp>

namespace respublica::protocol {

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

  for( const auto& operation: operations )
  {
    if( std::holds_alternative< upload_program >( operation ) )
    {
      if( !std::get< upload_program >( operation ).validate() )
        return false;
    }
    else if( std::holds_alternative< call_program >( operation ) )
    {
      if( !std::get< call_program >( operation ).validate() )
        return false;
    }
    else [[unlikely]]
      return false;
  }

  return true;
}

crypto::digest make_id( const transaction& t ) noexcept
{
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
      crypto::hasher_update( call.input.arguments );
      crypto::hasher_update( call.input.stdin );
    }
  }

  return crypto::hasher_finalize();
}

} // namespace respublica::protocol
