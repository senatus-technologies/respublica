add_library(crypto)

target_sources(crypto
  PUBLIC
    FILE_SET crypto_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/elliptic.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/merkle_tree.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/multihash.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/openssl.hpp
  PRIVATE
    elliptic.cpp
    multihash.cpp
    openssl.cpp)

target_link_libraries(crypto
  PRIVATE
    Koinos::exception
    Koinos::protocol
    Koinos::util
    OpenSSL::SSL
    OpenSSL::Crypto
    ethash::ethash
    secp256k1-vrf)

add_library(Koinos::crypto ALIAS crypto)

koinos_add_format(TARGET crypto)

add_executable(crypto_tests)

target_sources(crypto_tests
  PRIVATE
    elliptic.test.cpp
    merkle_tree.test.cpp
    multihash.test.cpp)

target_link_libraries(crypto_tests
  PRIVATE
    GTest::gtest
    GTest::gtest_main
    Koinos::crypto
    Koinos::exception
    Koinos::util)

koinos_add_format(TARGET crypto_tests)

gtest_discover_tests(crypto_tests)

