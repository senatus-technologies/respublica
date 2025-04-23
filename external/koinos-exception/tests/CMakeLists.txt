if (NOT BUILD_TESTING)
  return()
endif()

include(CTest)

koinos_add_test(
  exception_tests
    SOURCES
      exception_test.cpp
      main.cpp
      test_object.pb.cc
      test_object.pb.h)

target_link_libraries(exception_tests PRIVATE exception)

target_include_directories(
  exception_tests
    PRIVATE
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tests>)

koinos_add_format(
  TARGET
    exception_tests
  EXCLUDE
    test_object.pb.cc
    test_object.pb.h)

koinos_coverage(
  EXECUTABLE
    exception_tests
  EXCLUDE
    "tests/*")
