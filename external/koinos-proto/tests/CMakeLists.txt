if (NOT BUILD_TESTING)
  return()
endif()

include(CTest)

koinos_add_test(proto_tests
  SOURCES
    main.cpp
    proto_test.cpp
    test_map.pb.cc
    include/koinos/test_map.pb.h)

target_link_libraries(proto_tests PRIVATE proto)

target_include_directories(
  proto_tests
    PRIVATE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

koinos_add_format(
  TARGET
    proto_tests
  EXCLUDE
    test_map.pb.cc
    include/koinos/test_map.pb.h)

koinos_coverage(
  EXECUTABLE
    proto_tests
  EXCLUDE
    "./tests/*"
    "*.pb.cc"
    "*.pb.h")
