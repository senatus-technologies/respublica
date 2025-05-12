#include <benchmark/benchmark.h>
#include <cstdlib>
#include <koinos/log/log.hpp>
#include <koinos/tests/fixture.hpp>
#include <koinos/util/conversion.hpp>
#include <optional>

static std::unique_ptr< koinos::tests::fixture > fixture;

static koinos::rpc::chain::submit_transaction_request token_tx_req;
static koinos::rpc::chain::submit_transaction_request coin_tx_req;

static void token_transfers( benchmark::State& state )
{
  for( auto _: state )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->submit_transaction( token_tx_req );
  }

  state.counters[ "transfers" ] =
    benchmark::Counter( state.iterations() * state.threads(), benchmark::Counter::kIsRate );

  state.counters[ "transfer_time" ] = benchmark::Counter( state.iterations() * state.threads(),
                                                          benchmark::Counter::kIsRate | benchmark::Counter::kInvert );
}

BENCHMARK( token_transfers )->ThreadRange( 1, 16'384 )->UseRealTime()->MinWarmUpTime( 1 )->MinTime( 5 );

static void coin_transfers( benchmark::State& state )
{
  for( auto _: state )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->submit_transaction( coin_tx_req );
  }

  state.counters[ "transfers" ] =
    benchmark::Counter( state.iterations() * state.threads(), benchmark::Counter::kIsRate );

  state.counters[ "transfer_time" ] = benchmark::Counter( state.iterations() * state.threads(),
                                                          benchmark::Counter::kIsRate | benchmark::Counter::kInvert );
}

BENCHMARK( coin_transfers )->ThreadRange( 1, 16'384 )->UseRealTime()->MinWarmUpTime( 1 )->MinTime( 5 );

static void requests( benchmark::State& state )
{
  for( auto _: state )
  {
    [[maybe_unused]]
    auto response = fixture->_controller->get_head_info();
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

  *token_tx_req.mutable_transaction() =
    fixture->make_transaction( alice_secret_key,
                               1,
                               1'000'000,
                               fixture->make_transfer_operation(
                                 token_address,
                                 koinos::util::converter::as< std::string >( alice_secret_key.public_key().bytes() ),
                                 koinos::util::converter::as< std::string >( bob_secret_key.public_key().bytes() ),
                                 0 ) );

  *coin_tx_req.mutable_transaction() =
    fixture->make_transaction( alice_secret_key,
                               1,
                               1'000'000,
                               fixture->make_transfer_operation(
                                 "coin",
                                 koinos::util::converter::as< std::string >( alice_secret_key.public_key().bytes() ),
                                 koinos::util::converter::as< std::string >( bob_secret_key.public_key().bytes() ),
                                 0 ) );

  koinos::rpc::chain::submit_block_request block_req;
  *block_req.mutable_block() = fixture->make_block(
    *fixture->_block_signing_secret_key,
    fixture->make_transaction( token_secret_key,
                               1,
                               10'000'000,
                               fixture->make_upload_program_operation( token_address, get_koin_wasm() ) ) );

  return fixture->verify( fixture->_controller->submit_block( block_req ),
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
