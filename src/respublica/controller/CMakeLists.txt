add_library(controller)

target_sources(controller
  PUBLIC
    FILE_SET controller_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/respublica/controller.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/controller/controller.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/controller/error.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/controller/state.hpp
  PRIVATE
    FILE_SET controller_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      execution_context.hpp
      frame_recorder.hpp
      host_api.hpp
      program_stack.hpp
      resource_meter.hpp
      session.hpp
  PRIVATE
    controller.cpp
    error.cpp
    execution_context.cpp
    frame_recorder.cpp
    host_api.cpp
    program_stack.cpp
    resource_meter.cpp
    session.cpp
    state.cpp)

target_link_libraries(controller
  PUBLIC
    respublica::crypto
    respublica::encode
    respublica::memory
    respublica::program
    respublica::protocol
    respublica::state_db
    respublica::vm
  PRIVATE
    respublica::log
    $<$<BOOL:${FAST_MALLOC}>:gperftools::tcmalloc_minimal>)

add_library(respublica::controller ALIAS controller)

respublica_add_format(TARGET controller)

