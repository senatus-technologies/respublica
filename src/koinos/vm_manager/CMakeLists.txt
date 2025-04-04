add_library(vm_manager)

target_sources(vm_manager
  PUBLIC
    FILE_SET vm_manager_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/vm_manager/vm_backend.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/vm_manager/host_api.hpp
  PRIVATE
    FILE_SET vm_manager_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      fizzy/exceptions.hpp
      fizzy/fizzy_vm_backend.hpp
      fizzy/module_cache.hpp
      iwasm/exceptions.hpp
      iwasm/iwasm_vm_backend.hpp
      iwasm/module_cache.hpp
  PRIVATE
    host_api.cpp
    vm_backend.cpp
    timer.cpp
    fizzy/fizzy_vm_backend.cpp
    fizzy/module_cache.cpp
    iwasm/iwasm_vm_backend.cpp
    iwasm/module_cache.cpp)

target_link_libraries(vm_manager
  PRIVATE
    Koinos::exception
    Koinos::log
    Koinos::util
    fizzy::fizzy
    vmlib)

koinos_add_format(TARGET vm_manager)

