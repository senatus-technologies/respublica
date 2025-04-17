add_executable(benchmark)

target_sources(benchmark
  PRIVATE
    benchmark.cpp)

target_link_libraries(benchmark
  PRIVATE
    benchmark::benchmark
    Koinos::chain
    Koinos::protocol
    Koinos::crypto
    Koinos::state_db
    Koinos::log
    Koinos::util
    Koinos::mq
    Koinos::protocol
    Koinos::exception
    Koinos::fixture)

