add_library(crypto)

target_sources(crypto
  PUBLIC
    FILE_SET crypto_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/merkle_tree.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/multihash.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/openssl.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/public_key.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/secret_key.hpp
  PRIVATE
    multihash.cpp
    openssl.cpp
    public_key.cpp
    secret_key.cpp)

target_link_libraries(crypto
  PUBLIC
    nlohmann_json::nlohmann_json
    Koinos::util
    OpenSSL::Crypto
    protobuf::libprotobuf
  PRIVATE
    Boost::filesystem
    Koinos::error
    Koinos::protocol
    ethash::ethash
    secp256k1-vrf
    libsodium::libsodium)

add_library(Koinos::crypto ALIAS crypto)

koinos_add_format(TARGET crypto)

add_executable(crypto_tests)

target_sources(crypto_tests
  PRIVATE
    merkle_tree.test.cpp
    multihash.test.cpp
    public_key.test.cpp
    secret_key.test.cpp)

target_link_libraries(crypto_tests
  PRIVATE
    GTest::gtest
    GTest::gtest_main
    Koinos::crypto
    Koinos::proto)

koinos_add_format(TARGET crypto_tests)

gtest_discover_tests(crypto_tests)

