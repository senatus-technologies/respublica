add_library(error)

target_sources(error
  PUBLIC
    FILE_SET error_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/error/error.hpp
  PRIVATE
    error.cpp)

add_library(Koinos::error ALIAS error)

koinos_add_format(TARGET error)
