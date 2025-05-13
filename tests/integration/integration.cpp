#include <boost/endian/conversion.hpp>
#include <gtest/gtest.h>
#include <koinos/log/log.hpp>
#include <koinos/tests/fixture.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>

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
  std::string token_address = koinos::util::converter::as< std::string >( token_secret_key.public_key().bytes() );

  koinos::rpc::chain::submit_block_request block_req;
  *block_req.mutable_block() =
    make_block( *_block_signing_secret_key,
                make_transaction( token_secret_key,
                                  1,
                                  10'000'000,
                                  make_upload_program_operation( token_address, get_koin_wasm() ) ) );

  ASSERT_TRUE(
    verify( _controller->submit_block( block_req ),
            koinos::tests::fixture::verification::head | koinos::tests::fixture::verification::without_reversion ) );

  koinos::rpc::chain::submit_transaction_request tx_req;
  *tx_req.mutable_transaction() = make_transaction(
    *alice_secret_key,
    1,
    8'000'000,
    make_transfer_operation( token_address,
                             koinos::util::converter::as< std::string >( alice_secret_key->public_key().bytes() ),
                             koinos::util::converter::as< std::string >( bob_secret_key->public_key().bytes() ),
                             0 ) );
  ASSERT_TRUE(
    verify( _controller->submit_transaction( tx_req ), koinos::tests::fixture::verification::without_reversion ) );
}

TEST_F( integration, coin )
{
  koinos::rpc::chain::read_contract_request read_req;
  read_req.set_contract_id( "coin" );
  read_req.set_entry_point( koinos::tests::token_entry::name );
  auto response = _controller->read_contract( read_req );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( response->result(), "Coin" );

  read_req.set_entry_point( koinos::tests::token_entry::symbol );
  response = _controller->read_contract( read_req );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( response->result(), "COIN" );

  read_req.set_entry_point( koinos::tests::token_entry::decimals );
  response = _controller->read_contract( read_req );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 8 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result() ) ) );

  koinos::rpc::chain::submit_block_request block_req;
  *block_req.mutable_block() = make_block(
    *_block_signing_secret_key,
    make_transaction(
      *alice_secret_key,
      1,
      9'000'000,
      make_mint_operation( "coin",
                           koinos::util::converter::as< std::string >( alice_secret_key->public_key().bytes() ),
                           100 ) ) );

  ASSERT_TRUE(
    verify( _controller->submit_block( block_req ),
            koinos::tests::fixture::verification::head | koinos::tests::fixture::verification::without_reversion ) );

  read_req.set_entry_point( koinos::tests::token_entry::total_supply );
  response = _controller->read_contract( read_req );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 100 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result() ) ) );

  read_req.set_entry_point( koinos::tests::token_entry::balance_of );
  *read_req.add_args() = koinos::util::converter::as< std::string >( alice_secret_key->public_key().bytes() );
  response             = _controller->read_contract( read_req );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 100 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result() ) ) );

  *block_req.mutable_block() = make_block(
    *_block_signing_secret_key,
    make_transaction(
      *alice_secret_key,
      2,
      8'000'000,
      make_transfer_operation( "coin",
                               koinos::util::converter::as< std::string >( alice_secret_key->public_key().bytes() ),
                               koinos::util::converter::as< std::string >( bob_secret_key->public_key().bytes() ),
                               50 ) ) );

  ASSERT_TRUE(
    verify( _controller->submit_block( block_req ),
            koinos::tests::fixture::verification::head | koinos::tests::fixture::verification::without_reversion ) );

  read_req.set_entry_point( koinos::tests::token_entry::balance_of );
  read_req.clear_args();
  *read_req.add_args() = koinos::util::converter::as< std::string >( alice_secret_key->public_key().bytes() );
  response             = _controller->read_contract( read_req );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 50 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result() ) ) );

  read_req.set_entry_point( koinos::tests::token_entry::balance_of );
  read_req.clear_args();
  *read_req.add_args() = koinos::util::converter::as< std::string >( bob_secret_key->public_key().bytes() );
  response             = _controller->read_contract( read_req );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 50 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result() ) ) );

  *block_req.mutable_block() = make_block(
    *_block_signing_secret_key,
    make_transaction(
      *alice_secret_key,
      3,
      8'000'000,
      make_burn_operation( "coin",
                           koinos::util::converter::as< std::string >( alice_secret_key->public_key().bytes() ),
                           50 ) ) );

  ASSERT_TRUE(
    verify( _controller->submit_block( block_req ),
            koinos::tests::fixture::verification::head | koinos::tests::fixture::verification::without_reversion ) );

  read_req.set_entry_point( koinos::tests::token_entry::balance_of );
  read_req.clear_args();
  *read_req.add_args() = koinos::util::converter::as< std::string >( alice_secret_key->public_key().bytes() );
  response             = _controller->read_contract( read_req );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 0 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result() ) ) );

  read_req.set_entry_point( koinos::tests::token_entry::total_supply );
  read_req.clear_args();
  response = _controller->read_contract( read_req );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( uint64_t( 50 ),
             boost::endian::little_to_native( koinos::util::converter::to< uint64_t >( response->result() ) ) );
}
