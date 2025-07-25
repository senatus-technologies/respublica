add_library(encode)

target_sources(encode
  PUBLIC
    FILE_SET encode_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/respublica/encode.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/encode/base58.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/encode/error.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/encode/hex.hpp
  PRIVATE
    base58.cpp
    error.cpp
    hex.cpp)

target_link_libraries(encode
  PUBLIC
    respublica::memory)

add_library(respublica::encode ALIAS encode)

respublica_add_format(TARGET encode)

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
    respublica::encode)

respublica_add_format(TARGET encode_tests)

gtest_discover_tests(encode_tests)
