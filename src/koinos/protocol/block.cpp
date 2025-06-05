#include <koinos/protocol/block.hpp>

namespace koinos::protocol {

std::size_t block::size() const noexcept
{
  std::size_t bytes = 0;

  bytes += id.size();
  bytes += previous.size();
  bytes += sizeof( height );
  bytes += sizeof( timestamp );
  bytes += state_merkle_root.size();

  for( const auto& transaction: transactions )
    bytes += transaction.size();

  bytes += signer.size();
  bytes += signature.size();

  return bytes;
}

bool block::validate() const noexcept
{
  if( !height || !timestamp )
    return false;

  if( make_id( *this ) != id )
    return false;

  crypto::public_key key( signer );
  if( !key.verify( signature, id ) )
    return false;

  for( const auto& transaction: transactions )
    if( !transaction.validate() )
      return false;

  return true;
}

crypto::digest make_id( const block& b ) noexcept
{
  crypto::hasher_reset();

  crypto::hasher_update( b.previous );
  crypto::hasher_update( b.height );
  crypto::hasher_update( b.timestamp );
  crypto::hasher_update( b.state_merkle_root );

  for( const auto& transaction: b.transactions )
    crypto::hasher_update( transaction.id );

  crypto::hasher_update( b.signer );

  return crypto::hasher_finalize();
}

} // namespace koinos::protocol
