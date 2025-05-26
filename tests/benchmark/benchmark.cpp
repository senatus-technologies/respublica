#include <benchmark/benchmark.h>
#include <cstdlib>
#include <koinos/log/log.hpp>
#include <koinos/tests/fixture.hpp>
#include <koinos/util/conversion.hpp>
#include <optional>

static std::unique_ptr< koinos::tests::fixture > fixture;

static koinos::protocol::transaction coin_tx;
static koinos::protocol::transaction token_tx;

static void token_transactions( benchmark::State& state )
{
  for( auto _: state )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->process( token_tx );
  }

  state.counters[ "transactions" ] =
    benchmark::Counter( state.iterations() * state.threads(), benchmark::Counter::kIsRate );

  state.counters[ "transaction_time" ] =
    benchmark::Counter( state.iterations() * state.threads(),
                        benchmark::Counter::kIsRate | benchmark::Counter::kInvert );
}

BENCHMARK( token_transactions )->ThreadRange( 1, 16'384 )->UseRealTime()->MinWarmUpTime( 1 )->MinTime( 5 );

static void coin_transactions( benchmark::State& state )
{
  for( auto _: state )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->process( coin_tx );
  }

  state.counters[ "transactions" ] =
    benchmark::Counter( state.iterations() * state.threads(), benchmark::Counter::kIsRate );

  state.counters[ "transaction_time" ] =
    benchmark::Counter( state.iterations() * state.threads(),
                        benchmark::Counter::kIsRate | benchmark::Counter::kInvert );
}

BENCHMARK( coin_transactions )->ThreadRange( 1, 16'384 )->UseRealTime()->MinWarmUpTime( 1 )->MinTime( 5 );

static void requests( benchmark::State& state )
{
  for( auto _: state )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->head();
  }

  state.counters[ "requests" ] =
    benchmark::Counter( state.iterations() * state.threads(), benchmark::Counter::kIsRate );

  state.counters[ "request_time" ] = benchmark::Counter( state.iterations() * state.threads(),
                                                         benchmark::Counter::kIsRate | benchmark::Counter::kInvert );
}

BENCHMARK( requests )->ThreadRange( 1, 16'384 )->UseRealTime()->MinWarmUpTime( 1 )->MinTime( 5 );

// Benchmark setup routines and helper functions begin here.

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
                                       fixture->make_transfer_operation( koinos::protocol::system_account( "coin" ),
                                                                         alice_secret_key.public_key().bytes(),
                                                                         bob_secret_key.public_key().bytes(),
                                                                         0 ) );

  koinos::protocol::block block = fixture->make_block(
    *fixture->_block_signing_secret_key,
    fixture->make_transaction(
      token_secret_key,
      1,
      10'000'000,
      fixture->make_upload_program_operation( token_secret_key.public_key().bytes(), koin_program() ) ) );

  return fixture->verify( fixture->_controller->process( block ),
                          koinos::tests::fixture::verification::head
                            | koinos::tests::fixture::verification::without_reversion );
}

int main( int argc, char** argv )
{
  fixture = std::make_unique< koinos::tests::fixture >( "benchmark", "info" );
  if( !setup() )
    return EXIT_FAILURE;
  ::benchmark::Initialize( &argc, argv );
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  fixture = nullptr;
  return EXIT_SUCCESS;
}
