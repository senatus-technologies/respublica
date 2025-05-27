add_library(protocol INTERFACE)

target_sources(protocol
  INTERFACE
    FILE_SET protocol_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/block.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/operation.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/protocol.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/transaction.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/types.hpp)

target_link_libraries(protocol
  INTERFACE
    Boost::serialization)

add_library(Koinos::protocol ALIAS protocol)

