add_library(encode)

target_sources(encode
  PUBLIC
    FILE_SET encode_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/encode.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/encode/base58.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/encode/error.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/encode/hex.hpp
  PRIVATE
    base58.cpp
    error.cpp
    hex.cpp)

target_link_libraries(encode
  PUBLIC
    Koinos::memory)

add_library(Koinos::encode ALIAS encode)

koinos_add_format(TARGET encode)

add_executable(encode_tests)

target_sources(encode_tests
  PRIVATE
    base58.test.cpp
    hex.test.cpp)

target_link_libraries(encode_tests
  PRIVATE
    Boost::headers
    GTest::gtest
    GTest::gtest_main
    Koinos::encode)

koinos_add_format(TARGET encode_tests)

gtest_discover_tests(encode_tests)
