#include <ranges>

#include <boost/endian.hpp>
#include <boost/filesystem.hpp>

#include <koinos/controller.hpp>
#include <koinos/crypto.hpp>
#include <koinos/log.hpp>
#include <koinos/protocol.hpp>

#include <test/programs.hpp>

#include <filesystem>

namespace test {

enum token_entry : std::uint32_t
{
  name         = 0x82a3537f,
  symbol       = 0xb76a7ca1,
  decimals     = 0xee80fd2f,
  total_supply = 0xb0da3934,
  balance_of   = 0x5c721497,
  transfer     = 0x27f576ca,
  mint         = 0xdc6f17bb,
  burn         = 0x859facc5,
};

struct fixture
{
  fixture( const std::string& name, const std::string& log_level );
  ~fixture();

  koinos::protocol::operation make_upload_program_operation( const koinos::protocol::account& account,
                                                             const std::vector< std::byte >& bytecode );
  koinos::protocol::operation
  make_mint_operation( const koinos::protocol::account& id, const koinos::protocol::account& to, std::uint64_t amount );
  koinos::protocol::operation make_burn_operation( const koinos::protocol::account& id,
                                                   const koinos::protocol::account& from,
                                                   std::uint64_t amount );
  koinos::protocol::operation make_transfer_operation( const koinos::protocol::account& id,
                                                       const koinos::protocol::account& from,
                                                       const koinos::protocol::account& to,
                                                       std::uint64_t amount );

  template< Operation... Args >
  koinos::protocol::transaction
  make_transaction( const koinos::crypto::secret_key& signer, std::uint64_t nonce, std::uint64_t limit, Args... args )
  {
    koinos::protocol::transaction t;
    ( ( t.operations.emplace_back( std::forward< Args >( args ) ) ), ... );
    t.resource_limit = limit;
    t.network_id     = _controller->network_id();
    t.nonce          = nonce;
    t.payer          = signer.public_key().bytes();
    t.id             = koinos::protocol::make_id( t );

    koinos::protocol::authorization auth;
    auth.signer    = signer.public_key().bytes();
    auth.signature = signer.sign( t.id );

    t.authorizations.emplace_back( auth );

    return t;
  }

  template< Transaction... Args >
  koinos::protocol::block make_block( const koinos::crypto::secret_key& signer, Args... args )
  {
    auto head = _controller->head();
    auto now =
      std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now().time_since_epoch() )
        .count();
    std::uint64_t timestamp = head.time >= now ? head.time + 1 : now;
    return make_block( signer,
                       head.height + 1,
                       timestamp,
                       head.id,
                       head.state_merkle_root,
                       std::forward< Args >( args )... );
  }

  template< Transaction... Args >
  koinos::protocol::block make_block( const koinos::crypto::secret_key& signer,
                                      std::uint64_t height,
                                      std::uint64_t timestamp,
                                      const koinos::crypto::digest& previous,
                                      const koinos::crypto::digest& state_merkle_root,
                                      Args... args )
  {
    koinos::protocol::block b;
    ( ( b.transactions.emplace_back( std::forward< Args >( args ) ) ), ... );
    b.timestamp         = timestamp;
    b.height            = height;
    b.previous          = previous;
    b.state_merkle_root = state_merkle_root;
    b.signer            = signer.public_key().bytes();

    b.id        = koinos::protocol::make_id( b );
    b.signature = signer.sign( b.id );
    return b;
  }

  template< std::integral T >
  std::vector< std::byte > to_argument( T t ) const noexcept
  {
    boost::endian::native_to_little_inplace( t );
    auto byte_view = std::as_bytes( std::span( &t, 1 ) );
    return { byte_view.begin(), byte_view.end() };
  }

  template< std::ranges::range T >
  std::vector< std::byte > to_argument( const T& t ) const noexcept
  {
    auto byte_view = std::as_bytes( std::span( t ) );
    return { byte_view.begin(), byte_view.end() };
  }

  template< typename T, std::size_t N >
  std::vector< std::byte > to_argument( const std::array< T, N >& t ) const noexcept
  {
    return to_argument( std::ranges::views::all( t ) );
  }

  template< typename T >
    requires std::is_enum_v< T >
  std::vector< std::byte > to_argument( T t ) const noexcept
  {
    return to_argument( std::to_underlying( t ) );
  }

  std::vector< std::byte > to_argument( const koinos::crypto::public_key& k ) const noexcept
  {
    return to_argument( k.bytes() );
  }

  template< typename... Args >
  std::vector< std::vector< std::byte > > make_arguments( Args... args ) const noexcept
  {
    std::vector< std::vector< std::byte > > arguments;
    ( ( arguments.emplace_back( to_argument( std::forward< Args >( args ) ) ) ), ... );
    return arguments;
  }

  enum verification : std::uint8_t
  {
    none              = 0x00,
    processed         = 0x01,
    head              = 0x02,
    without_reversion = 0x04
  };

  bool verify( koinos::controller::result< koinos::protocol::block_receipt > receipt, std::uint64_t flags ) const;
  bool verify( koinos::controller::result< koinos::protocol::transaction_receipt > receipt, std::uint64_t flags ) const;

  std::unique_ptr< koinos::controller::controller > _controller;
  std::filesystem::path _state_dir;
  koinos::crypto::secret_key _block_signing_secret_key;
  koinos::controller::state::genesis_data _genesis_data;
};

} // namespace test
