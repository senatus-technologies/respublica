add_library(crypto)

target_sources(crypto
  PUBLIC
    FILE_SET crypto_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/hash.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/merkle_tree.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/public_key.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/public_key_view.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/crypto/secret_key.hpp
  PRIVATE
    hash.cpp
    public_key.cpp
    public_key_view.cpp
    secret_key.cpp)

target_link_libraries(crypto
  PRIVATE
    BLAKE3::blake3
    $<IF:$<BOOL:${FAST_CRYPTO}>,FourQ::fourq,libsodium::libsodium>
    Koinos::memory)

if(FAST_CRYPTO)
  target_compile_definitions(crypto
    PUBLIC
      -DFAST_CRYPTO)
endif()

add_library(Koinos::crypto ALIAS crypto)

koinos_add_format(TARGET crypto)

add_executable(crypto_tests)

target_sources(crypto_tests
  PRIVATE
    merkle_tree.test.cpp
    hash.test.cpp
    public_key.test.cpp
    public_key_view.test.cpp
    secret_key.test.cpp)

target_link_libraries(crypto_tests
  PRIVATE
    GTest::gtest
    GTest::gtest_main
    Koinos::crypto
    Koinos::encode)

koinos_add_format(TARGET crypto_tests)

gtest_discover_tests(crypto_tests)

