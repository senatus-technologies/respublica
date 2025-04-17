#include <benchmark/benchmark.h>

#include <koinos/tests/fixture.hpp>

static std::unique_ptr< koinos::tests::fixture > fixture;

static void transfers( benchmark::State& state )
{
  for( auto _: state )
  {
    fixture->_controller->submit_transaction( fixture->tx_req );
  }
  state.counters[ "Transactions" ] =
    benchmark::Counter( state.iterations() * state.threads(), benchmark::Counter::kIsRate );

  state.counters[ "Time" ] = benchmark::Counter( state.iterations() * state.threads(),
                                                 benchmark::Counter::kIsRate | benchmark::Counter::kInvert );
}

BENCHMARK( transfers )->ThreadRange( 1, 2'048 )->UseRealTime();

int main( int argc, char** argv )
{
  fixture = std::make_unique< koinos::tests::fixture >( "benchmark", "info" );
  ::benchmark::Initialize( &argc, argv );
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  fixture = nullptr;
  return 0;
}
