add_library(log)

target_sources(log
  PUBLIC
    FILE_SET log_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/log.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/log/log.hpp
  PRIVATE
    log.cpp)

target_link_libraries(log
  PUBLIC
    Boost::log_setup
    Boost::log
  PRIVATE
    Koinos::memory)

add_library(Koinos::log ALIAS log)

koinos_add_format(TARGET log)

add_executable(log_tests)

target_sources(log_tests
  PRIVATE
    log.test.cpp)

target_link_libraries(log_tests
  PRIVATE
    Boost::headers
    GTest::gtest
    GTest::gtest_main
    Koinos::log)

koinos_add_format(TARGET log_tests)

gtest_discover_tests(log_tests)

