add_library(protocol)

target_sources(protocol
  PUBLIC
    FILE_SET protocol_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/respublica/protocol.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/protocol/account.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/protocol/block.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/protocol/event.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/protocol/operation.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/protocol/program.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/protocol/transaction.hpp
  PRIVATE
    account.cpp
    block.cpp
    operation.cpp
    transaction.cpp)

target_link_libraries(protocol
  PUBLIC
    Boost::serialization
    respublica::crypto)

add_library(respublica::protocol ALIAS protocol)

respublica_add_format(TARGET protocol)
