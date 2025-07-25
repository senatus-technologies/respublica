#include <ranges>

#include <boost/endian.hpp>
#include <boost/filesystem.hpp>

#include <respublica/controller.hpp>
#include <respublica/crypto.hpp>
#include <respublica/memory.hpp>
#include <respublica/protocol.hpp>

#include <test/programs.hpp>

#include <filesystem>

namespace test {

namespace token {

enum instruction : std::uint32_t // NOLINT(performance-enum-size)
{
  authorize,
  name,
  symbol,
  decimals,
  total_supply,
  balance_of,
  transfer,
  mint,
  burn,
};

} // namespace token

struct fixture
{
  fixture( const fixture& )            = delete;
  fixture( fixture&& )                 = delete;
  fixture& operator=( const fixture& ) = delete;
  fixture& operator=( fixture&& )      = delete;
  fixture( const std::string& name, const std::string& log_level );
  ~fixture();

  respublica::protocol::operation make_upload_program_operation( const respublica::protocol::account& account,
                                                             const std::vector< std::byte >& bytecode );
  respublica::protocol::operation
  make_mint_operation( const respublica::protocol::account& id, const respublica::protocol::account& to, std::uint64_t amount );
  respublica::protocol::operation make_burn_operation( const respublica::protocol::account& id,
                                                   const respublica::protocol::account& from,
                                                   std::uint64_t amount );
  respublica::protocol::operation make_transfer_operation( const respublica::protocol::account& id,
                                                       const respublica::protocol::account& from,
                                                       const respublica::protocol::account& to,
                                                       std::uint64_t amount );

  template< Operation... Args >
  respublica::protocol::transaction
  make_transaction( const respublica::crypto::secret_key& signer, std::uint64_t nonce, std::uint64_t limit, Args... args )
  {
    respublica::protocol::transaction t;
    ( ( t.operations.emplace_back( std::forward< Args >( args ) ) ), ... );
    t.resource_limit = limit;
    t.network_id     = _controller->network_id();
    t.nonce          = nonce;
    t.payer          = respublica::protocol::user_account( signer.public_key() );
    t.id             = respublica::protocol::make_id( t );

    respublica::protocol::authorization auth;
    auth.signer    = respublica::protocol::user_account( signer.public_key() );
    auth.signature = signer.sign( t.id );

    t.authorizations.emplace_back( auth );

    return t;
  }

  template< Transaction... Args >
  respublica::protocol::block make_block( const respublica::crypto::secret_key& signer, Args... args )
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
  respublica::protocol::block make_block( const respublica::crypto::secret_key& signer,
                                      std::uint64_t height,
                                      std::uint64_t timestamp,
                                      const respublica::crypto::digest& previous,
                                      const respublica::crypto::digest& state_merkle_root,
                                      Args... args )
  {
    respublica::protocol::block b;
    ( ( b.transactions.emplace_back( std::forward< Args >( args ) ) ), ... );
    b.timestamp         = timestamp;
    b.height            = height;
    b.previous          = previous;
    b.state_merkle_root = state_merkle_root;
    b.signer            = respublica::protocol::user_account( signer.public_key() );
    b.id                = respublica::protocol::make_id( b );
    b.signature         = signer.sign( b.id );
    return b;
  }

  template< std::integral T >
  void append_stdin( std::vector< std::byte >& input, T t ) const noexcept
  {
    boost::endian::native_to_little_inplace( t );
    const auto bytes = respublica::memory::as_bytes( std::addressof( t ), 1 );
    input.insert( input.end(), bytes.begin(), bytes.end() );
  }

  template< std::ranges::range T >
  void append_stdin( std::vector< std::byte >& input, const T& t ) const noexcept
  {
    const auto bytes = respublica::memory::as_bytes( t );
    input.insert( input.end(), bytes.begin(), bytes.end() );
  }

  template< typename T, std::size_t N >
  void append_stdin( std::vector< std::byte >& input, const std::array< T, N >& t ) const noexcept
  {
    return append_stdin( input, std::ranges::views::all( t ) );
  }

  template< typename T >
    requires std::is_enum_v< T >
  void append_stdin( std::vector< std::byte >& input, T t ) const noexcept
  {
    return append_stdin( input, std::to_underlying( t ) );
  }

  void append_stdin( std::vector< std::byte >& input, const respublica::crypto::public_key& k ) const noexcept
  {
    return append_stdin( input, k.bytes() );
  }

  template< typename... Args >
  std::vector< std::byte > make_stdin( Args... args ) const noexcept
  {
    std::vector< std::byte > input;
    ( ( append_stdin( input, std::forward< Args >( args ) ) ), ... );
    return input;
  }

  respublica::protocol::program_input make_input( std::vector< std::byte >&& stdin,
                                              std::vector< std::string >&& arguments = {} ) const noexcept;

  enum verification : std::uint_fast8_t
  {
    none              = 0,
    processed         = 1 << 0,
    head              = 1 << 1,
    without_reversion = 1 << 2
  };

  bool verify( respublica::controller::result< respublica::protocol::block_receipt > receipt, std::uint64_t flags ) const;
  bool verify( respublica::controller::result< respublica::protocol::transaction_receipt > receipt, std::uint64_t flags ) const;

  std::unique_ptr< respublica::controller::controller > _controller;
  std::filesystem::path _state_dir;
  respublica::crypto::secret_key _block_signing_secret_key;
  respublica::controller::state::genesis_data _genesis_data;
};

} // namespace test
