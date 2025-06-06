add_library(protocol)

target_sources(protocol
  PUBLIC
    FILE_SET protocol_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/account.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/block.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/event.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/operation.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/program.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/protocol.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/transaction.hpp
  PRIVATE
    block.cpp
    operation.cpp
    transaction.cpp)

target_link_libraries(protocol
  PUBLIC
    Boost::serialization
    Koinos::crypto)

add_library(Koinos::protocol ALIAS protocol)

