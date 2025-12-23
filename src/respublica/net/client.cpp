#include <respublica/log.hpp>
#include <respublica/memory.hpp>
#include <respublica/net/client.hpp>
#include <respublica/net/peer.hpp>
#include <respublica/net/session.hpp>
#include <respublica/net/upnp.hpp>

#include <filesystem>
#include <string>

#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

namespace respublica::net {

client::client( boost::asio::io_context& io_context,
                std::uint16_t port,
                std::optional< boost::asio::ip::tcp::resolver::results_type > endpoints,
                const std::filesystem::path& cert_file,
                const std::filesystem::path& key_file ):
    _ioc( io_context ),
    _strand( boost::asio::make_strand( io_context ) ),
    _acceptor( io_context, boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4(), port ) ),
    _context( boost::asio::ssl::context::tlsv13 ),
    _private_key_path( key_file )
{
  // Check if certificate files exist, generate if missing
  if( !std::filesystem::exists( cert_file ) || !std::filesystem::exists( key_file ) )
  {
    LOG_WARNING( respublica::log::instance(),
                 "Certificate or key file not found. Generating self-signed certificate..." );

    if( !generate_certificate( cert_file, key_file ) )
    {
      LOG_ERROR( respublica::log::instance(), "Failed to generate self-signed certificate" );
    }
  }
  else
  {
    LOG_INFO( respublica::log::instance(), "Using certificate: {}, key: {}", cert_file.string(), key_file.string() );
  }

  // Configure context for both server and client mode
  _context.set_options( boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2
                        | boost::asio::ssl::context::single_dh_use );
  _context.use_certificate_chain_file( cert_file.string() );
  _context.use_private_key_file( key_file.string(), boost::asio::ssl::context::pem );
  _context.set_verify_mode( boost::asio::ssl::verify_peer );
  _context.load_verify_file( cert_file.string() );

  setup_upnp( port );
  do_accept();

  if( endpoints )
  {
    do_connect( *endpoints );
  }
}

client::~client()
{
  // Destructor must be defined in the .cpp file where upnp is complete
}

void client::do_accept()
{
  _acceptor.async_accept(
    [ this ]( const boost::system::error_code& error, boost::asio::ip::tcp::socket socket )
    {
      if( !error )
      {
        LOG_INFO( respublica::log::instance(), "Accepted incoming connection" );
        auto sess = std::make_shared< session >(
          boost::asio::ssl::stream< boost::asio::ip::tcp::socket >( std::move( socket ), _context ) );

        // Create peer with empty ID and connecting state
        // We'll set the actual peer ID after handshake
        auto p = std::make_shared< peer >( sess, peer_id{}, peer_state::handshaking );

        // Set handshake completion callback
        sess->on_handshake_complete(
          [ this, p ]( X509* peer_cert )
          {
            on_handshake_complete( p, peer_cert );
          } );

        // Add to connecting peers on strand for thread safety
        boost::asio::post( _strand,
                           [ this, p, sess ]()
                           {
                             _connecting_peers.push_back( p );
                             sess->start();
                           } );
      }
      else
      {
        LOG_ERROR( respublica::log::instance(), "Accept error: {}", error.message() );
      }

      do_accept();
    } );
}

void client::do_connect( const boost::asio::ip::tcp::resolver::results_type& endpoints )
{
  LOG_INFO( respublica::log::instance(), "Initiating connection to remote peer" );
  boost::asio::ssl::stream< boost::asio::ip::tcp::socket > socket( _ioc.get(), _context );
  auto sess = std::make_shared< session >( std::move( socket ) );

  // Create peer with empty ID and connecting state
  // We'll set the actual peer ID after handshake
  auto p = std::make_shared< peer >( sess, peer_id{}, peer_state::connecting );

  // Set handshake completion callback
  sess->on_handshake_complete(
    [ this, p ]( X509* peer_cert )
    {
      on_handshake_complete( p, peer_cert );
    } );

  // Add to connecting peers on strand for thread safety
  boost::asio::post( _strand,
                     [ this, p, sess, endpoints ]()
                     {
                       _connecting_peers.push_back( p );
                       sess->connect( endpoints );
                     } );
}

void client::setup_upnp( std::uint16_t port )
{
  try
  {
    _upnp   = std::make_unique< upnp >( _ioc );
    auto ec = _upnp->add_port_mapping( port, port, "TCP" );

    if( !ec )
    {
      // Successfully added port mapping, now get the external IP
      auto external_ip = _upnp->get_external_ip();
      if( external_ip )
      {
        LOG_INFO( respublica::log::instance(),
                  "UPnP port mapping successful. External IP: {}",
                  external_ip->to_string() );
      }
      else
      {
        LOG_WARNING( respublica::log::instance(),
                     "UPnP port mapping successful, but failed to get external IP: {}",
                     external_ip.error().message() );
      }
    }
    else
    {
      LOG_WARNING( respublica::log::instance(), "UPnP port mapping failed: {}", ec.message() );
    }
  }
  catch( const std::exception& e )
  {
    LOG_ERROR( respublica::log::instance(), "UPnP setup failed: {}", e.what() );
    _upnp.reset();
  }
}

// Generate a self-signed certificate and private key
bool client::generate_certificate( const std::string& cert_path, const std::string& key_path )
{
  LOG_INFO( respublica::log::instance(), "Generating self-signed certificate" );

  // Generate RSA key pair
  std::unique_ptr< EVP_PKEY, decltype( &EVP_PKEY_free ) > pkey( EVP_PKEY_new(), EVP_PKEY_free );
  if( !pkey )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to create EVP_PKEY" );
    return false;
  }

  std::unique_ptr< EVP_PKEY_CTX, decltype( &EVP_PKEY_CTX_free ) > ctx( EVP_PKEY_CTX_new_id( EVP_PKEY_RSA, nullptr ),
                                                                       EVP_PKEY_CTX_free );
  if( !ctx )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to create EVP_PKEY_CTX" );
    return false;
  }

  if( EVP_PKEY_keygen_init( ctx.get() ) <= 0 )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to initialize key generation" );
    return false;
  }

  constexpr int rsa_keygen_bits = 2'048;
  if( EVP_PKEY_CTX_set_rsa_keygen_bits( ctx.get(), rsa_keygen_bits ) <= 0 )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to set RSA key size" );
    return false;
  }

  EVP_PKEY* pkey_raw = nullptr;
  if( EVP_PKEY_keygen( ctx.get(), &pkey_raw ) <= 0 )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to generate RSA key" );
    return false;
  }
  pkey.reset( pkey_raw );

  // Create X509 certificate
  std::unique_ptr< X509, decltype( &X509_free ) > x509( X509_new(), X509_free );
  if( !x509 )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to create X509 certificate" );
    return false;
  }

  // Set certificate version (X509 v3)
  X509_set_version( x509.get(), 2 );

  // Set serial number
  ASN1_INTEGER_set( X509_get_serialNumber( x509.get() ), 1 );

  // Set validity period (never expires - set to year 9999)
  X509_gmtime_adj( X509_get_notBefore( x509.get() ), 0 );

  // Set expiration to December 31, 9999
  ASN1_TIME* not_after = X509_get_notAfter( x509.get() );
  ASN1_TIME_set_string( not_after, "99991231235959Z" );

  // Set public key
  X509_set_pubkey( x509.get(), pkey.get() );

  // Set subject name
  X509_NAME* name = X509_get_subject_name( x509.get() );
  X509_NAME_add_entry_by_txt( name,
                              "C",
                              MBSTRING_ASC,
                              memory::pointer_cast< const unsigned char* >( "US" ),
                              -1,
                              -1,
                              0 );
  X509_NAME_add_entry_by_txt( name,
                              "O",
                              MBSTRING_ASC,
                              memory::pointer_cast< const unsigned char* >( "Respublica" ),
                              -1,
                              -1,
                              0 );
  X509_NAME_add_entry_by_txt( name,
                              "CN",
                              MBSTRING_ASC,
                              memory::pointer_cast< const unsigned char* >( "localhost" ),
                              -1,
                              -1,
                              0 );

  // Set issuer name (same as subject for self-signed)
  X509_set_issuer_name( x509.get(), name );

  // Sign the certificate
  if( !X509_sign( x509.get(), pkey.get(), EVP_sha256() ) )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to sign certificate" );
    return false;
  }

  // Write certificate to file
  std::unique_ptr< FILE, decltype( &fclose ) > cert_file( fopen( cert_path.c_str(), "wb" ), fclose );
  if( !cert_file )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to open certificate file for writing: {}", cert_path );
    return false;
  }

  if( !PEM_write_X509( cert_file.get(), x509.get() ) )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to write certificate to file" );
    return false;
  }

  // Write private key to file
  std::unique_ptr< FILE, decltype( &fclose ) > key_file( fopen( key_path.c_str(), "wb" ), fclose );
  if( !key_file )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to open key file for writing: {}", key_path );
    return false;
  }

  if( !PEM_write_PrivateKey( key_file.get(), pkey.get(), nullptr, nullptr, 0, nullptr, nullptr ) )
  {
    LOG_ERROR( respublica::log::instance(), "Failed to write private key to file" );
    return false;
  }

  LOG_INFO( respublica::log::instance(), "Successfully generated self-signed certificate" );
  return true;
}

void client::register_global_handlers( const std::shared_ptr< peer >& p )
{
  // Apply all registered global handlers to the new peer
  for( const auto& [ type_id, handler ]: _global_handlers )
  {
    register_handler_on_peer( p, type_id );
  }
}

void client::register_handler_on_peer( const std::shared_ptr< peer >& p, message_type_id type_id )
{
  auto it = _global_handlers.find( type_id );
  if( it != _global_handlers.end() )
  {
    const auto& handler = it->second;
    p->session()->on_receive( type_id,
                              [ handler, p ]( std::span< const std::byte > data )
                              {
                                handler( p, data );
                              } );
  }
}

void client::on_handshake_complete( std::shared_ptr< peer > p, X509* peer_cert )
{
  // Execute on strand to ensure thread-safe access to peer vectors
  boost::asio::post(
    _strand,
    [ this, p, peer_cert ]()
    {
      if( !peer_cert )
      {
        LOG_ERROR( respublica::log::instance(), "Handshake completed but no peer certificate available" );
        p->set_state( peer_state::failed );

        // Remove from connecting peers
        _connecting_peers.erase( std::remove( _connecting_peers.begin(), _connecting_peers.end(), p ),
                                 _connecting_peers.end() );
        return;
      }

      // Extract peer ID from certificate
      peer_id id = extract_peer_id_from_certificate( peer_cert );

      if( id == peer_id{} )
      {
        LOG_ERROR( respublica::log::instance(), "Failed to extract peer ID from certificate" );
        p->set_state( peer_state::failed );

        // Remove from connecting peers
        _connecting_peers.erase( std::remove( _connecting_peers.begin(), _connecting_peers.end(), p ),
                                 _connecting_peers.end() );
        return;
      }

      // Set the peer ID and mark as ready
      p->set_id( id );
      p->set_state( peer_state::ready );

      LOG_INFO( respublica::log::instance(), "Peer handshake complete. Peer ID: {}", peer_id_to_string( id ) );

      // Move from connecting to connected peers
      _connecting_peers.erase( std::remove( _connecting_peers.begin(), _connecting_peers.end(), p ),
                               _connecting_peers.end() );

      _peers.push_back( p );

      // Register global handlers now that peer is ready
      register_global_handlers( p );
    } );
}

} // namespace respublica::net
