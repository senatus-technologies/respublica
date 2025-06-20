add_library(controller)

target_sources(controller
  PUBLIC
    FILE_SET controller_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/controller.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/controller/controller.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/controller/error.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/controller/state.hpp
  PRIVATE
    FILE_SET controller_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      call_stack.hpp
      chronicler.hpp
      execution_context.hpp
      host_api.hpp
      resource_meter.hpp
      session.hpp
  PRIVATE
    call_stack.cpp
    chronicler.cpp
    controller.cpp
    error.cpp
    execution_context.cpp
    host_api.cpp
    resource_meter.cpp
    session.cpp
    state.cpp)

target_link_libraries(controller
  PUBLIC
    Koinos::crypto
    Koinos::encode
    Koinos::memory
    Koinos::program
    Koinos::protocol
    Koinos::state_db
    Koinos::vm_manager
  PRIVATE
    Koinos::log
    $<$<BOOL:${FAST_MALLOC}>:gperftools::tcmalloc_minimal>)

add_library(Koinos::controller ALIAS controller)

koinos_add_format(TARGET controller)

