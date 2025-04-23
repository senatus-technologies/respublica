add_library(protocol)

target_sources(protocol
  PUBLIC
    FILE_SET protocol_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/serialization.hpp
  PRIVATE
    serialization.cpp)

target_link_libraries(protocol
  PUBLIC
    Koinos::proto)

add_library(Koinos::protocol ALIAS protocol)

koinos_add_format(TARGET protocol)

add_executable(protocol_tests)

target_sources(protocol_tests
  PRIVATE
    serialization.test.cpp)

target_link_libraries(protocol_tests
  PRIVATE
    GTest::gtest
    GTest::gtest_main
    Koinos::protocol)

koinos_add_format(TARGET protocol_tests)

gtest_discover_tests(protocol_tests)

