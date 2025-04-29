add_executable(integration_tests)

target_sources(integration_tests
  PRIVATE
    integration.cpp)

target_link_libraries(integration_tests
  PRIVATE
    GTest::gtest
    GTest::gtest_main
    Koinos::fixture)

gtest_discover_tests(integration_tests)

koinos_add_format(TARGET integration_tests)

