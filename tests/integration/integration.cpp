#include <boost/endian/conversion.hpp>
#include <gtest/gtest.h>
#include <koinos/log/log.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>
#include <ranges>
#include <test/fixture.hpp>

class integration: public ::testing::Test,
                   public test::fixture
{
public:
  integration():
      test::fixture( "integration", "trace" )
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
                      make_upload_program_operation( token_secret_key.public_key().bytes(), koin_program() ) ) );

  ASSERT_TRUE( verify( _controller->process( block ),
                       test::fixture::verification::head | test::fixture::verification::without_reversion ) );

  koinos::protocol::transaction transaction =
    make_transaction( *alice_secret_key,
                      1,
                      8'000'000,
                      make_transfer_operation( token_secret_key.public_key().bytes(),
                                               alice_secret_key->public_key().bytes(),
                                               bob_secret_key->public_key().bytes(),
                                               0 ) );
  ASSERT_TRUE( verify( _controller->process( transaction ), test::fixture::verification::without_reversion ) );
}

TEST_F( integration, coin )
{
  koinos::protocol::account coin = koinos::protocol::system_account( "coin" );

  auto response = _controller->read_program( coin, test::token_entry::name );

  ASSERT_TRUE( response.has_value() );

  std::string_view name( reinterpret_cast< const char* >( response->result.data() ), response->result.size() );
  EXPECT_EQ( name, "Coin" );

  response = _controller->read_program( coin, test::token_entry::symbol );

  ASSERT_TRUE( response.has_value() );

  std::string_view symbol( reinterpret_cast< const char* >( response->result.data() ), response->result.size() );
  EXPECT_EQ( symbol, "COIN" );

  response = _controller->read_program( coin, test::token_entry::decimals );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 8 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );

  koinos::protocol::block block =
    make_block( *_block_signing_secret_key,
                make_transaction( *alice_secret_key,
                                  1,
                                  9'000'000,
                                  make_mint_operation( coin, alice_secret_key->public_key().bytes(), 100 ) ) );

  ASSERT_TRUE( verify( _controller->process( block ),
                       test::fixture::verification::head | test::fixture::verification::without_reversion ) );

  response = _controller->read_program( coin, test::token_entry::total_supply );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 100 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );

  std::vector< std::vector< std::byte > > arguments;
  arguments.emplace_back( alice_secret_key->public_key().bytes().begin(), alice_secret_key->public_key().bytes().end() );
  response = _controller->read_program( coin, test::token_entry::balance_of, arguments );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 100 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );

  block = make_block( *_block_signing_secret_key,
                      make_transaction( *alice_secret_key,
                                        2,
                                        8'000'000,
                                        make_transfer_operation( coin,
                                                                 alice_secret_key->public_key().bytes(),
                                                                 bob_secret_key->public_key().bytes(),
                                                                 50 ) ) );

  ASSERT_TRUE( verify( _controller->process( block ),
                       test::fixture::verification::head | test::fixture::verification::without_reversion ) );

  response = _controller->read_program( coin, test::token_entry::balance_of, arguments );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 50 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );

  arguments.clear();
  arguments.emplace_back( bob_secret_key->public_key().bytes().begin(), bob_secret_key->public_key().bytes().begin() );
  response = _controller->read_program( coin, test::token_entry::balance_of, arguments );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 50 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );

  block = make_block( *_block_signing_secret_key,
                      make_transaction( *alice_secret_key,
                                        3,
                                        8'000'000,
                                        make_burn_operation( coin, alice_secret_key->public_key().bytes(), 50 ) ) );

  ASSERT_TRUE( verify( _controller->process( block ),
                       test::fixture::verification::head | test::fixture::verification::without_reversion ) );

  arguments.clear();
  arguments.emplace_back( alice_secret_key->public_key().bytes().begin(), alice_secret_key->public_key().bytes().end() );
  response = _controller->read_program( coin, test::token_entry::balance_of, arguments );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 0 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );

  response = _controller->read_program( coin, test::token_entry::total_supply );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 50 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result ) ) );
}
