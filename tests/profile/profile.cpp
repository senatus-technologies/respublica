#include <cstdlib>
#include <gperftools/heap-profiler.h>
#include <gperftools/profiler.h>
#include <koinos/log/log.hpp>
#include <koinos/tests/fixture.hpp>
#include <koinos/util/conversion.hpp>
#include <optional>

static std::unique_ptr< koinos::tests::fixture > fixture;

static koinos::protocol::transaction coin_tx;
static koinos::protocol::transaction token_tx;

static bool setup()
{
  auto alice_secret_key = *koinos::crypto::secret_key::create(
    *koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, std::string( "alice" ) ) );
  auto bob_secret_key = *koinos::crypto::secret_key::create(
    *koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, std::string( "bob" ) ) );

  auto token_secret_key = *koinos::crypto::secret_key::create(
    *koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, std::string( "token" ) ) );
  auto token_address = koinos::util::converter::as< std::string >( token_secret_key.public_key().bytes() );

  token_tx = fixture->make_transaction( alice_secret_key,
                                        1,
                                        1'000'000,
                                        fixture->make_transfer_operation( token_secret_key.public_key().bytes(),
                                                                          alice_secret_key.public_key().bytes(),
                                                                          bob_secret_key.public_key().bytes(),
                                                                          0 ) );

  coin_tx = fixture->make_transaction( alice_secret_key,
                                       1,
                                       1'000'000,
                                       fixture->make_transfer_operation( koinos::protocol::account_from_name( "coin" ),
                                                                         alice_secret_key.public_key().bytes(),
                                                                         bob_secret_key.public_key().bytes(),
                                                                         0 ) );

  koinos::protocol::block block = fixture->make_block(
    *fixture->_block_signing_secret_key,
    fixture->make_transaction(
      token_secret_key,
      1,
      10'000'000,
      fixture->make_upload_program_operation( token_secret_key.public_key().bytes(), get_koin_wasm() ) ) );

  return fixture->verify( fixture->_controller->process( block ),
                          koinos::tests::fixture::verification::head
                            | koinos::tests::fixture::verification::without_reversion );
}

int main( int arc, char** argv )
{
  fixture = std::make_unique< koinos::tests::fixture >( "profile", "info" );

  if( !setup() )
    return EXIT_FAILURE;

  ProfilerStart( "token.transactions.cpu.out" );
  for( int i = 0; i < 10'000; i++ )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->process( token_tx );
  }
  ProfilerStop();

  HeapProfilerStart( "token.transactions" );
  for( int i = 0; i < 10'000; i++ )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->process( token_tx );
  }
  HeapProfilerStop();

  ProfilerStart( "coin.transactions.cpu.out" );
  for( int i = 0; i < 10'000; i++ )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->process( coin_tx );
  }
  ProfilerStop();

  HeapProfilerStart( "coin.transactions" );
  for( int i = 0; i < 10'000; i++ )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->process( coin_tx );
  }
  HeapProfilerStop();

  ProfilerStart( "requests.cpu.out" );
  for( int i = 0; i < 10'000; i++ )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->head();
  }
  ProfilerStop();

  HeapProfilerStart( "requests" );
  for( int i = 0; i < 10'000; i++ )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->head();
  }
  HeapProfilerStop();

  fixture = nullptr;
  return EXIT_SUCCESS;
}
