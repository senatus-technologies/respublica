#include <boost/endian/conversion.hpp>
#include <gtest/gtest.h>
#include <koinos/log/log.hpp>
#include <koinos/tests/fixture.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <ranges>

class integration: public ::testing::Test,
                   public koinos::tests::fixture
{
public:
  integration():
      koinos::tests::fixture( "integration", "trace" )
  {}

  ~integration() = default;

protected:
  std::optional< koinos::crypto::secret_key > alice_secret_key;
  std::optional< koinos::crypto::secret_key > bob_secret_key;

  virtual void SetUp()
  {
    alice_secret_key = *koinos::crypto::secret_key::create(
      *koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, std::string( "alice" ) ) );
    bob_secret_key = *koinos::crypto::secret_key::create(
      *koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, std::string( "bob" ) ) );

    LOG( info ) << "Alice public key: " << koinos::util::to_base58( alice_secret_key->public_key().bytes() );
    LOG( info ) << "Bob public key: " << koinos::util::to_base58( bob_secret_key->public_key().bytes() );
  }
};

TEST_F( integration, token )
{
  auto token_secret_key = *koinos::crypto::secret_key::create(
    *koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, std::string( "token" ) ) );

  koinos::protocol::block block = make_block(
    *_block_signing_secret_key,
    make_transaction( token_secret_key,
                      1,
                      10'000'000,
                      make_upload_program_operation( token_secret_key.public_key().bytes(), get_koin_wasm() ) ) );

  ASSERT_TRUE(
    verify( _controller->process( block ),
            koinos::tests::fixture::verification::head | koinos::tests::fixture::verification::without_reversion ) );

  koinos::protocol::transaction transaction =
    make_transaction( *alice_secret_key,
                      1,
                      8'000'000,
                      make_transfer_operation( token_secret_key.public_key().bytes(),
                                               alice_secret_key->public_key().bytes(),
                                               bob_secret_key->public_key().bytes(),
                                               0 ) );
  ASSERT_TRUE( verify( _controller->process( transaction ), koinos::tests::fixture::verification::without_reversion ) );
}

TEST_F( integration, coin )
{
  koinos::protocol::account coin = koinos::protocol::account_from_name( "coin" );

  auto as_string = []( std::byte b )
  {
    return static_cast< const char >( b );
  };

  auto response = _controller->read_program( coin, koinos::tests::token_entry::name );

  ASSERT_TRUE( response.has_value() );

  std::string_view name( reinterpret_cast< const char* >( response->result.data() ), response->result.size() );
  EXPECT_EQ( name, "Coin" );

  response = _controller->read_program( coin, koinos::tests::token_entry::symbol );

  ASSERT_TRUE( response.has_value() );

  std::string_view symbol( reinterpret_cast< const char* >( response->result.data() ), response->result.size() );
  EXPECT_EQ( symbol, "COIN" );

  response = _controller->read_program( coin, koinos::tests::token_entry::decimals );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 8 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );

  koinos::protocol::block block =
    make_block( *_block_signing_secret_key,
                make_transaction( *alice_secret_key,
                                  1,
                                  9'000'000,
                                  make_mint_operation( coin, alice_secret_key->public_key().bytes(), 100 ) ) );

  ASSERT_TRUE(
    verify( _controller->process( block ),
            koinos::tests::fixture::verification::head | koinos::tests::fixture::verification::without_reversion ) );

  response = _controller->read_program( coin, koinos::tests::token_entry::total_supply );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 100 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );

  std::vector< std::vector< std::byte > > arguments;
  arguments.emplace_back( std::ranges::to< std::vector< std::byte > >( alice_secret_key->public_key().bytes() ) );
  response = _controller->read_program( coin, koinos::tests::token_entry::balance_of, arguments );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 100 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );

  block = make_block( *_block_signing_secret_key,
                      make_transaction( *alice_secret_key,
                                        2,
                                        8'000'000,
                                        make_transfer_operation( koinos::protocol::account_from_name( "coin" ),
                                                                 alice_secret_key->public_key().bytes(),
                                                                 bob_secret_key->public_key().bytes(),
                                                                 50 ) ) );

  ASSERT_TRUE(
    verify( _controller->process( block ),
            koinos::tests::fixture::verification::head | koinos::tests::fixture::verification::without_reversion ) );

  response = _controller->read_program( coin, koinos::tests::token_entry::balance_of, arguments );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 50 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );

  arguments.clear();
  arguments.emplace_back( std::ranges::to< std::vector< std::byte > >( bob_secret_key->public_key().bytes() ) );
  response = _controller->read_program( coin, koinos::tests::token_entry::balance_of, arguments );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 50 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );

  block = make_block( *_block_signing_secret_key,
                      make_transaction( *alice_secret_key,
                                        3,
                                        8'000'000,
                                        make_burn_operation( coin, alice_secret_key->public_key().bytes(), 50 ) ) );

  ASSERT_TRUE(
    verify( _controller->process( block ),
            koinos::tests::fixture::verification::head | koinos::tests::fixture::verification::without_reversion ) );

  arguments.clear();
  arguments.emplace_back( std::ranges::to< std::vector< std::byte > >( alice_secret_key->public_key().bytes() ) );
  response = _controller->read_program( coin, koinos::tests::token_entry::balance_of, arguments );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 0 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );

  response = _controller->read_program( coin, koinos::tests::token_entry::total_supply );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 50 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );
}
