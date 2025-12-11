add_library(net)

target_sources(net
  PUBLIC
    FILE_SET net_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/respublica/net.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/net/client.hpp
  PRIVATE
    FILE_SET net_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      session.hpp
  PRIVATE
    client.cpp
    session.cpp)

add_library(respublica::net ALIAS net)

target_link_libraries(net
  PUBLIC
    Boost::headers
    openssl::openssl)

respublica_add_format(TARGET net)
