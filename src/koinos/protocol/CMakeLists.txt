add_library(protocol)

target_sources(protocol
  PUBLIC
    FILE_SET protocol_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/serialization.hpp
  PRIVATE
    serialization.cpp)

target_link_libraries(protocol
  PUBLIC
    Koinos::proto)

add_library(Koinos::protocol ALIAS protocol)

koinos_add_format(TARGET protocol)

