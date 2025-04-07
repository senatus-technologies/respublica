cmake_minimum_required(VERSION 3.19.0)

cmake_policy(VERSION 3.19.0...3.27.4)

set(HUNTER_PACKAGES
  Boost
  ethash
  libsecp256k1-vrf
  nlohmann_json
  OpenSSL
  rabbitmq-c
  rocksdb
  yaml-cpp
  Protobuf
  fizzy
  gRPC
  abseil
  re2
  c-ares
  zstd
  libxml2
  ZLIB
  LLVM
  koinos_log
  koinos_proto
  koinos_exception
  koinos_crypto
  koinos_mq
  koinos_state_db
  koinos_util)
set(HUNTER_Boost_COMPONENTS system log thread date_time filesystem chrono test program_options exception)
set(HUNTER_FILEPATH_CONFIG ${CMAKE_CURRENT_LIST_DIR}/tools/cmake/config.cmake)

include(FetchContent)
FetchContent_Declare(SetupHunter GIT_REPOSITORY https://github.com/cpp-pm/gate)
FetchContent_MakeAvailable(SetupHunter)

project(koinos_chain
  VERSION 1.4.1
  DESCRIPTION "Koinos Chain microservice"
  LANGUAGES CXX C)

find_package(Boost CONFIG REQUIRED system log log_setup thread date_time filesystem chrono program_options exception)
find_package(ethash CONFIG REQUIRED)
find_package(libsecp256k1-vrf CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(rabbitmq-c CONFIG REQUIRED)
find_package(RocksDB CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(fizzy CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(re2 CONFIG REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(ZLIB CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(libxml2 CONFIG REQUIRED)
find_package(LLVM CONFIG REQUIRED)

#find_package(koinos_log CONFIG REQUIRED)
find_package(koinos_util CONFIG REQUIRED)
find_package(koinos_proto CONFIG REQUIRED)
find_package(koinos_exception CONFIG REQUIRED)
#find_package(koinos_crypto CONFIG REQUIRED)
find_package(koinos_mq CONFIG REQUIRED)
#find_package(koinos_state_db CONFIG REQUIRED)

add_subdirectory(external)

include(tools/cmake/Koinos.cmake)

koinos_define_version()

add_subdirectory(src)
add_subdirectory(tests)
