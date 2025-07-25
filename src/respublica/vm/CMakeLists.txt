add_library(vm)

target_sources(vm
  PUBLIC
    FILE_SET vm_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/respublica/vm.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/vm/error.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/vm/host_api.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/vm/virtual_machine.hpp
  PRIVATE
    FILE_SET vm_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      module_cache.hpp
      program_context.hpp
  PRIVATE
    error.cpp
    module_cache.cpp
    program_context.cpp
    virtual_machine.cpp)

target_link_libraries(vm
  PRIVATE
    respublica::log
    respublica::memory
    fizzy::fizzy)

add_library(respublica::vm ALIAS vm)

respublica_add_format(TARGET vm)

