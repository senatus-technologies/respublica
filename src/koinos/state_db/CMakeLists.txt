add_library(state_db)

target_sources(state_db
  PUBLIC
    FILE_SET state_db_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/database.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/state_db.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/state_node.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/types.hpp
  PRIVATE
    FILE_SET state_db_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      delta_index.hpp
      state_delta.hpp
      backends/backend.hpp
      backends/iterator.hpp
      backends/map/map_backend.hpp
      backends/map/map_iterator.hpp
      backends/map/types.hpp
#      backends/rocksdb/object_cache.hpp
#      backends/rocksdb/rocksdb_backend.hpp
#      backends/rocksdb/rocksdb_iterator.hpp
  PRIVATE
    database.cpp
    delta_index.cpp
    permanent_state_node.cpp
    state_delta.cpp
    state_node.cpp
    temporary_state_node.cpp
    backends/backend.cpp
    backends/iterator.cpp
    backends/map/map_backend.cpp
    backends/map/map_iterator.cpp)
#    backends/rocksdb/rocksdb_backend.cpp
#    backends/rocksdb/rocksdb_iterator.cpp
#    backends/rocksdb/object_cache.cpp)

target_link_libraries(state_db
  PRIVATE
    Boost::headers
    Koinos::crypto
    RocksDB::rocksdb)

add_library(Koinos::state_db ALIAS state_db)

koinos_add_format(TARGET state_db)

add_executable(state_db_tests)

target_include_directories(state_db_tests
  PRIVATE
    $<TARGET_PROPERTY:Koinos::state_db,INCLUDE_DIRECTORIES>)

target_sources(state_db_tests
  PRIVATE
    state_delta.test.cpp
    backends/backend.test.cpp
    backends/map/map_backend.test.cpp)

target_link_libraries(state_db_tests
  PRIVATE
    GTest::gtest
    GTest::gtest_main
    Koinos::crypto
    Koinos::state_db)

koinos_add_format(TARGET state_db_tests)

gtest_discover_tests(state_db_tests)

