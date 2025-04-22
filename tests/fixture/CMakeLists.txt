add_library(fixture)

target_sources(fixture
  PUBLIC
    FILE_SET fixture_headers
    TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/contracts.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/fixture.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/util.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/authorize.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/benchmark.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/call.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/contract_return.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/db_write.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/echo.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/empty_contract.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/exit.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/forever.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/get_arguments_override.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/hello.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/koin.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/null_bytes_written.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/syscall_override.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/syscall_rpc.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/stack/call_contract.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/stack/call_system_call.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/stack/call_system_call2.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/stack/stack_assertion.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/stack/system_from_system.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/stack/system_from_user.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/stack/user_from_system.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/koinos/tests/wasm/stack/user_from_user.hpp
  PRIVATE
    contracts.cpp
    fixture.cpp)

target_link_libraries(fixture
  PUBLIC
    Boost::filesystem
    Boost::system
    nlohmann_json::nlohmann_json
    Koinos::chain
    Koinos::crypto
    Koinos::protocol
    Koinos::log
    Koinos::util
    protobuf::protobuf
  PRIVATE
    Koinos::state_db)

add_library(Koinos::fixture ALIAS fixture)

koinos_add_format(TARGET fixture)

