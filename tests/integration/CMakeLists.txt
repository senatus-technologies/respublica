add_executable(integration_tests)

target_sources(integration_tests
  PRIVATE
    integration.cpp)

target_link_libraries(integration_tests
  PRIVATE
    GTest::gtest
    GTest::gtest_main
    respublica::fixture)

gtest_discover_tests(integration_tests)

respublica_add_format(TARGET integration_tests)

