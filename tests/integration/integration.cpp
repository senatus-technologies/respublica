// NOLINTBEGIN

#include <boost/endian/conversion.hpp>
#include <gtest/gtest.h>
#include <respublica/log.hpp>
#include <respublica/memory.hpp>
#include <respublica/program.hpp>
#include <test/fixture.hpp>

class integration: public ::testing::Test,
                   public test::fixture
{
public:
  integration():
      test::fixture( "integration", "trace" ),
      alice_secret_key( respublica::crypto::secret_key::create( respublica::crypto::hash( "alice" ) ) ),
      bob_secret_key( respublica::crypto::secret_key::create( respublica::crypto::hash( "bob" ) ) )
  {}

  integration( const integration& ) = delete;
  integration( integration&& )      = delete;

  ~integration() override = default;

  integration& operator=( const integration& ) = delete;
  integration& operator=( integration&& )      = delete;

  respublica::crypto::secret_key alice_secret_key;
  respublica::crypto::secret_key bob_secret_key;
};

TEST_F( integration, token )
{
  auto token_secret_key = respublica::crypto::secret_key::create( respublica::crypto::hash( "token" ) );

  respublica::protocol::block block =
    make_block( _block_signing_secret_key,
                make_transaction( token_secret_key,
                                  1,
                                  10'000'000,
                                  make_upload_program_operation(
                                    respublica::protocol::program_account( token_secret_key.public_key().bytes() ),
                                    token_program() ) ) );

  ASSERT_TRUE( verify( _controller->process( block ),
                       test::fixture::verification::head | test::fixture::verification::without_reversion ) );

  respublica::protocol::account token = respublica::protocol::program_account( token_secret_key.public_key() );

  auto response = _controller->read_program( token, make_input( make_stdin( test::token::instruction::name ) ) );

  ASSERT_TRUE( response.has_value() );

  std::string_view name( respublica::memory::pointer_cast< const char* >( response->stdout.data() ),
                         response->stdout.size() );
  EXPECT_EQ( name, "Token" );

  response = _controller->read_program( token, make_input( make_stdin( test::token::instruction::symbol ) ) );

  ASSERT_TRUE( response.has_value() );

  std::string_view symbol( respublica::memory::pointer_cast< const char* >( response->stdout.data() ),
                           response->stdout.size() );
  EXPECT_EQ( symbol, "TOKEN" );

  response = _controller->read_program( token, make_input( make_stdin( test::token::instruction::decimals ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint32_t( 8 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint32_t >( response->stdout ) ) );

  block = make_block(
    _block_signing_secret_key,
    make_transaction(
      alice_secret_key,
      1,
      9'000'000,
      make_mint_operation( token, respublica::protocol::user_account( alice_secret_key.public_key() ), 100 ) ) );

  ASSERT_TRUE( verify( _controller->process( block ),
                       test::fixture::verification::head | test::fixture::verification::without_reversion ) );

  response = _controller->read_program( token, make_input( make_stdin( test::token::instruction::total_supply ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint64_t( 100 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint64_t >( response->stdout ) ) );

  response = _controller->read_program(
    token,
    make_input( make_stdin( test::token::instruction::balance_of,
                            respublica::protocol::user_account( alice_secret_key.public_key() ) ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint64_t( 100 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint64_t >( response->stdout ) ) );

  block = make_block(
    _block_signing_secret_key,
    make_transaction( alice_secret_key,
                      2,
                      8'000'000,
                      make_transfer_operation( token,
                                               respublica::protocol::user_account( alice_secret_key.public_key() ),
                                               respublica::protocol::user_account( bob_secret_key.public_key() ),
                                               50 ) ) );

  ASSERT_TRUE( verify( _controller->process( block ),
                       test::fixture::verification::head | test::fixture::verification::without_reversion ) );

  response = _controller->read_program(
    token,
    make_input( make_stdin( test::token::instruction::balance_of,
                            respublica::protocol::user_account( alice_secret_key.public_key() ) ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint64_t( 50 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint64_t >( response->stdout ) ) );

  response = _controller->read_program(
    token,
    make_input( make_stdin( test::token::instruction::balance_of,
                            respublica::protocol::user_account( bob_secret_key.public_key() ) ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint64_t( 50 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint64_t >( response->stdout ) ) );

  block = make_block(
    _block_signing_secret_key,
    make_transaction(
      alice_secret_key,
      3,
      8'000'000,
      make_burn_operation( token, respublica::protocol::user_account( alice_secret_key.public_key() ), 50 ) ) );

  ASSERT_TRUE( verify( _controller->process( block ),
                       test::fixture::verification::head | test::fixture::verification::without_reversion ) );

  response = _controller->read_program(
    token,
    make_input( make_stdin( test::token::instruction::balance_of, alice_secret_key.public_key() ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint64_t( 0 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint64_t >( response->stdout ) ) );

  response = _controller->read_program( token, make_input( make_stdin( test::token::instruction::total_supply ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint64_t( 50 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint64_t >( response->stdout ) ) );

  response =
    _controller->read_program( token, make_input( make_stdin( std::numeric_limits< std::uint64_t >::max() ) ) );
  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( response->code, std::to_underlying( respublica::program::program_errc::invalid_instruction ) );
  EXPECT_TRUE( !response->stdout.size() );
  EXPECT_TRUE( !response->stderr.size() );
}

TEST_F( integration, coin )
{
  respublica::protocol::account coin = respublica::protocol::system_program( "coin" );

  auto response = _controller->read_program( coin, make_input( make_stdin( test::token::instruction::name ) ) );

  ASSERT_TRUE( response.has_value() );

  std::string_view name( respublica::memory::pointer_cast< const char* >( response->stdout.data() ),
                         response->stdout.size() );
  EXPECT_EQ( name, "Coin" );

  response = _controller->read_program( coin, make_input( make_stdin( test::token::instruction::symbol ) ) );

  ASSERT_TRUE( response.has_value() );

  std::string_view symbol( respublica::memory::pointer_cast< const char* >( response->stdout.data() ),
                           response->stdout.size() );
  EXPECT_EQ( symbol, "COIN" );

  response = _controller->read_program( coin, make_input( make_stdin( test::token::instruction::decimals ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint32_t( 8 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint32_t >( response->stdout ) ) );

  respublica::protocol::block block = make_block(
    _block_signing_secret_key,
    make_transaction(
      alice_secret_key,
      1,
      9'000'000,
      make_mint_operation( coin, respublica::protocol::user_account( alice_secret_key.public_key().bytes() ), 100 ) ) );

  ASSERT_TRUE( verify( _controller->process( block ),
                       test::fixture::verification::head | test::fixture::verification::without_reversion ) );

  response = _controller->read_program( coin, make_input( make_stdin( test::token::instruction::total_supply ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint64_t( 100 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint64_t >( response->stdout ) ) );

  response = _controller->read_program(
    coin,
    make_input( make_stdin( test::token::instruction::balance_of,
                            respublica::protocol::user_account( alice_secret_key.public_key() ) ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint64_t( 100 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint64_t >( response->stdout ) ) );

  block = make_block( _block_signing_secret_key,
                      make_transaction( alice_secret_key,
                                        2,
                                        8'000'000,
                                        make_transfer_operation(
                                          coin,
                                          respublica::protocol::user_account( alice_secret_key.public_key().bytes() ),
                                          respublica::protocol::user_account( bob_secret_key.public_key().bytes() ),
                                          50 ) ) );

  ASSERT_TRUE( verify( _controller->process( block ),
                       test::fixture::verification::head | test::fixture::verification::without_reversion ) );

  response = _controller->read_program(
    coin,
    make_input( make_stdin( test::token::instruction::balance_of,
                            respublica::protocol::user_account( alice_secret_key.public_key() ) ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint64_t( 50 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint64_t >( response->stdout ) ) );

  response = _controller->read_program(
    coin,
    make_input( make_stdin( test::token::instruction::balance_of,
                            respublica::protocol::user_account( bob_secret_key.public_key() ) ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint64_t( 50 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint64_t >( response->stdout ) ) );

  block = make_block(
    _block_signing_secret_key,
    make_transaction(
      alice_secret_key,
      3,
      8'000'000,
      make_burn_operation( coin, respublica::protocol::user_account( alice_secret_key.public_key().bytes() ), 50 ) ) );

  ASSERT_TRUE( verify( _controller->process( block ),
                       test::fixture::verification::head | test::fixture::verification::without_reversion ) );

  response = _controller->read_program(
    coin,
    make_input( make_stdin( test::token::instruction::balance_of,
                            respublica::protocol::user_account( alice_secret_key.public_key() ) ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint64_t( 0 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint64_t >( response->stdout ) ) );

  response = _controller->read_program( coin, make_input( make_stdin( test::token::instruction::total_supply ) ) );

  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( std::uint64_t( 50 ),
             boost::endian::little_to_native( respublica::memory::bit_cast< std::uint64_t >( response->stdout ) ) );

  response = _controller->read_program( coin, make_input( make_stdin( std::numeric_limits< std::uint64_t >::max() ) ) );
  ASSERT_TRUE( response.has_value() );
  EXPECT_EQ( response->code, std::to_underlying( respublica::program::program_errc::invalid_instruction ) );
  EXPECT_TRUE( !response->stdout.size() );
  EXPECT_TRUE( !response->stderr.size() );
}

// NOLINTEND
