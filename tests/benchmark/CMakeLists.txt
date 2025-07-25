add_executable(benchmark)

target_sources(benchmark
  PRIVATE
    benchmark.cpp)

target_link_libraries(benchmark
  PRIVATE
    benchmark::benchmark
    respublica::fixture)

respublica_add_format(TARGET benchmark)

