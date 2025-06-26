add_library(vm_manager)

target_sources(vm_manager
  PUBLIC
    FILE_SET vm_manager_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/vm.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/vm/error.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/vm/host_api.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/vm/virtual_machine.hpp
  PRIVATE
    FILE_SET vm_manager_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      module_cache.hpp
  PRIVATE
    error.cpp
    module_cache.cpp
    virtual_machine.cpp)

target_link_libraries(vm_manager
  PRIVATE
    Koinos::log
    Koinos::memory
    fizzy::fizzy)

add_library(Koinos::vm_manager ALIAS vm_manager)

koinos_add_format(TARGET vm_manager)

