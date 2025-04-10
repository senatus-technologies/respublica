file(GLOB_RECURSE GENERATED "*.pb.cc" "${PROJECT_SOURCE_DIR}/include/*.pb.h")

add_library(proto
  koinos/serialization.cpp

  ${PROJECT_SOURCE_DIR}/include/koinos/serialization.hpp

  ${GENERATED})

target_link_libraries(
  proto
    PUBLIC
      protobuf::libprotobuf
      gRPC::grpc)

add_library(Koinos::proto ALIAS proto)

target_include_directories(
  proto
    PUBLIC
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>)

install(
  DIRECTORY
    ${PROJECT_SOURCE_DIR}/include
  DESTINATION
    ${CMAKE_INSTALL_PREFIX})
