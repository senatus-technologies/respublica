cmake_minimum_required(VERSION 3.24)

cmake_policy(VERSION 3.19.0...3.27.4)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")

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
set(CMAKE_CXX_MODULE_STD 1)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
#add_compile_options(-stdlib=libc++)

add_subdirectory(external)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/tools/cmake/module)
include(Format)
include(Git)

koinos_add_git_target()

add_subdirectory(src)

if (BUILD_TESTS)
  add_subdirectory(tests)
endif()


