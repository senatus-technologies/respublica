add_library(program)

target_sources(program
  PUBLIC
    FILE_SET program_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/respublica/program.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/program/coin.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/program/error.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/program/program.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/program/system_interface.hpp
  PRIVATE
    coin.cpp
    error.cpp)

target_link_libraries(program
  PUBLIC
    respublica::protocol)

add_library(respublica::program ALIAS program)

respublica_add_format(TARGET program)
