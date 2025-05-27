#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <koinos/chain/controller.hpp>
#include <koinos/crypto/crypto.hpp>
#include <koinos/protocol/protocol.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/hex.hpp>

#include <test/programs.hpp>

#include <filesystem>
#include <optional>

namespace test {

enum token_entry : uint32_t
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

  void set_block_merkle_roots( koinos::protocol::block& block,
                               koinos::crypto::multicodec code,
                               koinos::crypto::digest_size size = koinos::crypto::digest_size( 0 ) );
  void sign_block( koinos::protocol::block& block, koinos::crypto::secret_key& block_signing_key );
  void set_transaction_merkle_roots( koinos::protocol::transaction& transaction,
                                     koinos::crypto::multicodec code,
                                     koinos::crypto::digest_size size = koinos::crypto::digest_size( 0 ) );
  void add_signature( koinos::protocol::transaction& transaction, koinos::crypto::secret_key& transaction_signing_key );
  void sign_transaction( koinos::protocol::transaction& transaction,
                         const koinos::crypto::secret_key& transaction_signing_key );

  koinos::protocol::operation make_upload_program_operation( const koinos::protocol::account& account,
                                                             const std::vector< std::byte >& bytecode );
  koinos::protocol::operation
  make_mint_operation( const koinos::protocol::account& id, const koinos::protocol::account& to, uint64_t amount );
  koinos::protocol::operation
  make_burn_operation( const koinos::protocol::account& id, const koinos::protocol::account& from, uint64_t amount );
  koinos::protocol::operation make_transfer_operation( const koinos::protocol::account& id,
                                                       const koinos::protocol::account& from,
                                                       const koinos::protocol::account& to,
                                                       uint64_t amount );

  template< Operation... Args >
  koinos::protocol::transaction
  make_transaction( const koinos::crypto::secret_key& signer, uint64_t nonce, uint64_t limit, Args... args )
  {
    koinos::protocol::transaction t;
    ( ( t.operations.emplace_back( std::forward< Args >( args ) ) ), ... );
    t.header.resource_limit = limit;
    t.header.network_id     = _controller->network_id();
    t.header.nonce          = nonce;
    set_transaction_merkle_roots( t, koinos::crypto::multicodec::sha2_256 );
    sign_transaction( t, signer );
    return t;
  }

  template< Transaction... Args >
  koinos::protocol::block make_block( const koinos::crypto::secret_key& signer, Args... args )
  {
    auto head = _controller->head();
    auto now =
      std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now().time_since_epoch() )
        .count();
    uint64_t timestamp = head.time >= now ? head.time + 1 : now;
    return make_block( signer,
                       head.height + 1,
                       timestamp,
                       head.id,
                       head.state_merkle_root,
                       std::forward< Args >( args )... );
  }

  template< Transaction... Args >
  koinos::protocol::block make_block( const koinos::crypto::secret_key& signer,
                                      uint64_t height,
                                      uint64_t timestamp,
                                      const koinos::protocol::digest& previous,
                                      const koinos::protocol::digest& previous_state_merkle_root,
                                      Args... args )
  {
    koinos::protocol::block b;
    ( ( b.transactions.emplace_back( std::forward< Args >( args ) ) ), ... );
    b.header.timestamp                  = timestamp;
    b.header.height                     = height;
    b.header.previous                   = previous;
    b.header.previous_state_merkle_root = previous_state_merkle_root;
    set_block_merkle_roots( b, koinos::crypto::multicodec::sha2_256 );
    auto id = koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, b.header )->digest();
    assert( id.size() == b.id.size() );
    std::copy( id.begin(), id.end(), b.id.begin() );
    sign_block( b, *_block_signing_secret_key );
    return b;
  }

  enum verification : uint64_t
  {
    none              = 0x00,
    processed         = 0x01,
    head              = 0x02,
    without_reversion = 0x04
  };

  bool verify( std::expected< koinos::protocol::block_receipt, koinos::error::error > receipt, uint64_t flags ) const;
  bool verify( std::expected< koinos::protocol::transaction_receipt, koinos::error::error > receipt,
               uint64_t flags ) const;

  std::unique_ptr< koinos::chain::controller > _controller;
  std::filesystem::path _state_dir;
  std::optional< koinos::crypto::secret_key > _block_signing_secret_key;
  koinos::chain::state::genesis_data _genesis_data;
};

} // namespace test
