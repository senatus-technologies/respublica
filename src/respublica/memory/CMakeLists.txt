add_library(memory INTERFACE)

target_sources(memory
  INTERFACE
    FILE_SET memory_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/respublica/memory.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/memory/memory.hpp)

add_library(respublica::memory ALIAS memory)

#respublica_add_format(TARGET memory)
