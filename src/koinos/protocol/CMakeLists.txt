add_library(protocol INTERFACE)

target_sources(protocol
  INTERFACE
    FILE_SET protocol_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/block.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/operation.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/transaction.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/protocol/types.hpp)

target_link_libraries(protocol
  INTERFACE
    Boost::serialization)

add_library(Koinos::protocol ALIAS protocol)

#koinos_add_format(TARGET protocol)

#add_executable(protocol_tests)

#target_sources(protocol_tests
#  PRIVATE
#    serialization.test.cpp)

#target_link_libraries(protocol_tests
#  PRIVATE
#    GTest::gtest
#    GTest::gtest_main
#    Koinos::protocol)

#koinos_add_format(TARGET protocol_tests)

#gtest_discover_tests(protocol_tests)

