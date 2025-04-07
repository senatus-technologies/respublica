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
    Koinos::proto
    Koinos::util
    OpenSSL::SSL
    OpenSSL::Crypto
    ethash::ethash
    secp256k1-vrf::secp256k1-vrf)

add_library(Koinos::crypto ALIAS crypto)

koinos_add_format(TARGET crypto)

