add_library(chain)

target_sources(chain
  PUBLIC
    FILE_SET chain_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/controller.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/error.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/state.hpp
  PRIVATE
    FILE_SET chain_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      call_stack.hpp
      chronicler.hpp
      coin.hpp
      execution_context.hpp
      host_api.hpp
      program.hpp
      resource_meter.hpp
      session.hpp
      system_interface.hpp
  PRIVATE
    call_stack.cpp
    chronicler.cpp
    coin.cpp
    controller.cpp
    error.cpp
    execution_context.cpp
    host_api.cpp
    resource_meter.cpp
    session.cpp
    state.cpp)

target_link_libraries(chain
  PUBLIC
    Koinos::crypto
    Koinos::protocol
    Koinos::state_db
    Koinos::util
    Koinos::vm_manager
  PRIVATE
    Koinos::log
    $<$<BOOL:${FAST_MALLOC}>:gperftools::tcmalloc_minimal>)

add_library(Koinos::chain ALIAS chain)

koinos_add_format(TARGET chain)

