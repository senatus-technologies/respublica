add_library(chain)

target_sources(chain
  PUBLIC
    FILE_SET chain_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/controller.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/state.hpp
  PRIVATE
    FILE_SET chain_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      call_stack.hpp
      chronicler.hpp
      constants.hpp
      execution_context.hpp
      host_api.hpp
      resource_meter.hpp
      session.hpp
      system_interface.hpp
      types.hpp
  PRIVATE
    call_stack.cpp
    chronicler.cpp
    controller.cpp
    execution_context.cpp
    host_api.cpp
    resource_meter.cpp
    session.cpp
    state.cpp)

target_link_libraries(chain
  PUBLIC
    Koinos::crypto
    Koinos::proto
    Koinos::protocol
    Koinos::state_db
    Koinos::util
    Koinos::vm_manager
  PRIVATE
    Koinos::log
    gperftools::tcmalloc_minimal)

add_library(Koinos::chain ALIAS chain)

koinos_add_format(TARGET chain)

