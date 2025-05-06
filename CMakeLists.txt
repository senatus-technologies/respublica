cmake_minimum_required(VERSION 3.24)

cmake_policy(VERSION 3.19.0...3.27.4)

project(koinos_chain
  VERSION 1.4.1
  DESCRIPTION "Koinos Chain microservice"
  LANGUAGES CXX C)

option(BUILD_TESTS "Build tests" ON)

find_package(Boost CONFIG REQUIRED)
find_package(ethash CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(rabbitmq-c CONFIG REQUIRED)
find_package(RocksDB CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gperftools CONFIG REQUIRED)
find_package(wasm-micro-runtime CONFIG REQUIRED)
find_package(libsodium REQUIRED)

if (BUILD_TESTS)
  find_package(benchmark REQUIRED)
  find_package(GTest REQUIRED)
  include(GoogleTest)
  enable_testing()
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(external)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/tools/cmake/module)
include(Format)
include(Git)

koinos_add_git_target()

add_subdirectory(data)
add_subdirectory(src)

if (BUILD_TESTS)
  add_subdirectory(tests)
endif()

