#include <respublica/net/peer.hpp>

#include <blake3.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <respublica/crypto.hpp>
#include <respublica/encode.hpp>
#include <respublica/log.hpp>
#include <respublica/memory.hpp>

namespace respublica::net {

std::string peer_id_to_string( const peer_id& id )
{
  return encode::to_base58( id );
}

peer_id generate_peer_id( const std::string& private_key_path )
{
  // Read private key from file
  std::unique_ptr< FILE, decltype( &fclose ) > key_file( fopen( private_key_path.c_str(), "rb" ), fclose );
  if( !key_file )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to open private key file: {}", private_key_path );
    return peer_id{};
  }

  // Read the private key
  std::unique_ptr< EVP_PKEY, decltype( &EVP_PKEY_free ) > pkey(
    PEM_read_PrivateKey( key_file.get(), nullptr, nullptr, nullptr ),
    EVP_PKEY_free );

  if( !pkey )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to read private key from file: {}", private_key_path );
    return peer_id{};
  }

  // Get the raw key data
  std::size_t key_len = 0;
  if( EVP_PKEY_get_raw_private_key( pkey.get(), nullptr, &key_len ) != 1 )
  {
    // For RSA keys, we need to export the key in a different way
    std::unique_ptr< BIO, decltype( &BIO_free ) > bio( BIO_new( BIO_s_mem() ), BIO_free );
    if( !bio )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to create BIO for key export" );
      return peer_id{};
    }

    if( PEM_write_bio_PrivateKey( bio.get(), pkey.get(), nullptr, nullptr, 0, nullptr, nullptr ) != 1 )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to export private key" );
      return peer_id{};
    }

    // Get the PEM data
    BUF_MEM* mem = nullptr;
    BIO_get_mem_ptr( bio.get(), &mem );

    // Hash the PEM representation using BLAKE3
    blake3_hasher hasher;
    blake3_hasher_init( &hasher );
    blake3_hasher_update( &hasher, mem->data, mem->length );

    peer_id id;
    blake3_hasher_finalize( &hasher, memory::pointer_cast< uint8_t* >( id.data() ), id.size() );

    return id;
  }

  // For raw keys (e.g., Ed25519), we can get the raw bytes directly
  std::vector< unsigned char > key_data( key_len );
  if( EVP_PKEY_get_raw_private_key( pkey.get(), key_data.data(), &key_len ) != 1 )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to extract raw private key" );
    return peer_id{};
  }

  // Hash the key data using BLAKE3 to generate UUID
  blake3_hasher hasher;
  blake3_hasher_init( &hasher );
  blake3_hasher_update( &hasher, key_data.data(), key_len );

  peer_id id;
  blake3_hasher_finalize( &hasher, memory::pointer_cast< uint8_t* >( id.data() ), id.size() );

  return id;
}

peer::peer( std::shared_ptr< net::session > sess, peer_id id ):
    _session( std::move( sess ) ),
    _id( id )
{}

} // namespace respublica::net
