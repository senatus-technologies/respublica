cmake_minimum_required(VERSION 3.24)

cmake_policy(VERSION 3.19.0...3.27.4)

project(respublica
  VERSION 0.1.0
  DESCRIPTION "A high performance decentralized turing-complete public ledger"
  LANGUAGES CXX C)

option(BUILD_TESTS "Build tests" ON)
option(FAST_CRYPTO "Use FourQ public key cryptography" ON)
option(FAST_MALLOC "Link to TCMalloc" ON)

find_package(Boost CONFIG REQUIRED)
find_package(RocksDB CONFIG REQUIRED)
find_package(gperftools CONFIG REQUIRED)
find_package(libsodium REQUIRED)
find_package(quill CONFIG REQUIRED)
find_package(OpenSSL CONFIG REQUIRED)
find_package(libxml2 REQUIRED)

if (BUILD_TESTS)
  find_package(benchmark REQUIRED)
  find_package(GTest REQUIRED)
  include(GoogleTest)
  enable_testing()
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(external)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/tools/cmake/module)
include(Format)
include(Git)

respublica_add_git_target()

add_subdirectory(src)

if (BUILD_TESTS)
  add_subdirectory(tests)
endif()
