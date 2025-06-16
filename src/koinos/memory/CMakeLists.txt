add_library(memory INTERFACE)

target_sources(memory
  INTERFACE
    FILE_SET memory_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/memory/memory.hpp)

add_library(Koinos::memory ALIAS memory)

#koinos_add_format(TARGET memory)
