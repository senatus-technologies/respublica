add_library(program)

target_sources(program
  PUBLIC
    FILE_SET program_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/program.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/program/coin.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/program/error.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/program/program.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/program/system_interface.hpp
  PRIVATE
    coin.cpp
    error.cpp)

target_link_libraries(program
  PUBLIC
    Koinos::protocol)

add_library(Koinos::program ALIAS program)

koinos_add_format(TARGET program)
