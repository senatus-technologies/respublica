add_library(log)

target_sources(log
  PUBLIC
    FILE_SET log_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/log/log.hpp
  PRIVATE
    log.cpp)

target_link_libraries(log
  PRIVATE
    Boost::log_setup
    Boost::log
    protobuf::libprotobuf
    Koinos::util)

add_library(Koinos::log ALIAS log)

koinos_add_format(TARGET log)

