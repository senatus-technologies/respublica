// NOLINTBEGIN

#include <benchmark/benchmark.h>
#include <cstdlib>
#include <koinos/log/log.hpp>
#include <test/fixture.hpp>

static std::unique_ptr< test::fixture > fixture;

static koinos::protocol::transaction coin_tx;
static koinos::protocol::transaction token_tx;

constexpr auto min_threads     = 1;
constexpr auto max_threads     = 1 << 14;
constexpr auto min_warmup_time = 1;
constexpr auto min_time        = 5;

static void token_transactions( benchmark::State& state )
{
  for( auto _: state )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->process( token_tx );
  }

  state.counters[ "transactions" ] =
    benchmark::Counter( double( state.iterations() * state.threads() ), benchmark::Counter::kIsRate );

  state.counters[ "transaction_time" ] =
    benchmark::Counter( double( state.iterations() * state.threads() ),
                        benchmark::Counter::kIsRate | benchmark::Counter::kInvert );
}

BENCHMARK( token_transactions )
  ->ThreadRange( min_threads, max_threads )
  ->UseRealTime()
  ->MinWarmUpTime( min_warmup_time )
  ->MinTime( min_time );

static void coin_transactions( benchmark::State& state )
{
  for( auto _: state )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->process( coin_tx );
  }

  state.counters[ "transactions" ] =
    benchmark::Counter( double( state.iterations() * state.threads() ), benchmark::Counter::kIsRate );

  state.counters[ "transaction_time" ] =
    benchmark::Counter( double( state.iterations() * state.threads() ),
                        benchmark::Counter::kIsRate | benchmark::Counter::kInvert );
}

BENCHMARK( coin_transactions )
  ->ThreadRange( min_threads, max_threads )
  ->UseRealTime()
  ->MinWarmUpTime( min_warmup_time )
  ->MinTime( min_time );

static void requests( benchmark::State& state )
{
  for( auto _: state )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->head();
  }

  state.counters[ "requests" ] =
    benchmark::Counter( double( state.iterations() * state.threads() ), benchmark::Counter::kIsRate );

  state.counters[ "request_time" ] = benchmark::Counter( double( state.iterations() * state.threads() ),
                                                         benchmark::Counter::kIsRate | benchmark::Counter::kInvert );
}

BENCHMARK( requests )
  ->ThreadRange( min_threads, max_threads )
  ->UseRealTime()
  ->MinWarmUpTime( min_warmup_time )
  ->MinTime( min_time );

// Benchmark setup routines and helper functions begin here.

static bool setup()
{
  auto alice_secret_key = koinos::crypto::secret_key::create( koinos::crypto::hash( "alice" ) );
  auto bob_secret_key   = koinos::crypto::secret_key::create( koinos::crypto::hash( "bob" ) );
  auto token_secret_key = koinos::crypto::secret_key::create( koinos::crypto::hash( "token" ) );

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
    fixture->_block_signing_secret_key,
    fixture->make_transaction(
      token_secret_key,
      1,
      10'000'000,
      fixture->make_upload_program_operation( token_secret_key.public_key().bytes(), koin_program() ) ) );

  return fixture->verify( fixture->_controller->process( block ),
                          test::fixture::verification::head | test::fixture::verification::without_reversion );
}

int main( int argc, char** argv )
{
  fixture = std::make_unique< test::fixture >( "benchmark", "info" );
  if( !setup() )
    return EXIT_FAILURE;
  ::benchmark::Initialize( &argc, argv );
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  fixture = nullptr;
  return EXIT_SUCCESS;
}

// NOLINTEND
