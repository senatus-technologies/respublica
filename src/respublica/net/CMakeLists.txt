add_library(net)

target_sources(net
  PUBLIC
    FILE_SET net_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/respublica/net.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/net/client.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/net/error.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/net/message.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/net/peer.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/net/protocol.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/net/session.hpp
  PRIVATE
    FILE_SET net_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      upnp.hpp
  PRIVATE
    client.cpp
    error.cpp
    message.cpp
    peer.cpp
    session.cpp
    upnp.cpp)

add_library(respublica::net ALIAS net)

target_link_libraries(net
  PUBLIC
    Boost::headers
    Boost::serialization
    Boost::url
    respublica::log
    respublica::protocol
    respublica::crypto
  PRIVATE
    openssl::openssl
    respublica::memory
    respublica::encode
    LibXml2::LibXml2
    BLAKE3::blake3)

respublica_add_format(TARGET net)

add_executable(net_tests)

target_sources(net_tests
  PRIVATE
    message.test.cpp
    peer.test.cpp
    protocol.test.cpp)

target_link_libraries(net_tests
  PRIVATE
    GTest::gtest
    GTest::gtest_main
    respublica::net)

respublica_add_format(TARGET net_tests)

gtest_discover_tests(net_tests)

