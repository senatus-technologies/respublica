#include <respublica/net/peer.hpp>

#include <blake3.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <respublica/crypto.hpp>
#include <respublica/encode.hpp>
#include <respublica/log.hpp>
#include <respublica/memory.hpp>

namespace respublica::net {

namespace {

// Helper function to hash an EVP_PKEY's public key to generate peer_id
peer_id hash_public_key( EVP_PKEY* pkey )
{
  if( !pkey )
  {
    LOG_ERROR( respublica::log::instance(), "Cannot hash null public key" );
    return peer_id{};
  }

  // Export the public key to DER format
  std::unique_ptr< BIO, decltype( &BIO_free ) > bio( BIO_new( BIO_s_mem() ), BIO_free );
  if( !bio )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to create BIO for public key export" );
    return peer_id{};
  }

  if( i2d_PUBKEY_bio( bio.get(), pkey ) <= 0 )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to export public key to DER format" );
    return peer_id{};
  }

  // Get the DER data
  BUF_MEM* mem = nullptr;
  BIO_get_mem_ptr( bio.get(), &mem );

  // Hash the DER representation using BLAKE3
  blake3_hasher hasher;
  blake3_hasher_init( &hasher );
  blake3_hasher_update( &hasher, mem->data, mem->length );

  peer_id id;
  blake3_hasher_finalize( &hasher, memory::pointer_cast< uint8_t* >( id.data() ), id.size() );

  return id;
}

} // namespace

std::string peer_id_to_string( const peer_id& id )
{
  return encode::to_base58( id );
}

peer_id generate_peer_id( const std::filesystem::path& private_key_path )
{
  // Read private key from file
  std::unique_ptr< FILE, decltype( &fclose ) > key_file( fopen( private_key_path.string().c_str(), "rb" ), fclose );
  if( !key_file )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to open private key file: {}", private_key_path.string() );
    return peer_id{};
  }

  // Read the private key
  std::unique_ptr< EVP_PKEY, decltype( &EVP_PKEY_free ) > pkey(
    PEM_read_PrivateKey( key_file.get(), nullptr, nullptr, nullptr ),
    EVP_PKEY_free );

  if( !pkey )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to read private key from file: {}", private_key_path.string() );
    return peer_id{};
  }

  // Hash the public key portion to generate peer ID
  return hash_public_key( pkey.get() );
}

peer::peer( std::shared_ptr< net::session > sess, peer_id id, peer_state initial_state ):
    _session( std::move( sess ) ),
    _id( id ),
    _state( initial_state )
{}

peer_id extract_peer_id_from_certificate( X509* cert )
{
  if( !cert )
  {
    LOG_ERROR( respublica::log::instance(), "Cannot extract peer ID from null certificate" );
    return peer_id{};
  }

  // Get the public key from the certificate
  std::unique_ptr< EVP_PKEY, decltype( &EVP_PKEY_free ) > pkey( X509_get_pubkey( cert ), EVP_PKEY_free );
  if( !pkey )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to extract public key from certificate" );
    return peer_id{};
  }

  // Hash the public key to generate peer ID
  return hash_public_key( pkey.get() );
}

} // namespace respublica::net
