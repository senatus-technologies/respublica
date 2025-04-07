add_library(state_db)

target_sources(state_db
  PUBLIC
    FILE_SET state_db_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/state_db_types.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/state_db.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/backends/backend.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/backends/exceptions.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/backends/iterator.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/backends/types.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/backends/map/map_backend.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/backends/map/map_iterator.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/backends/rocksdb/exceptions.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/backends/rocksdb/object_cache.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/backends/rocksdb/rocksdb_backend.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/backends/rocksdb/rocksdb_iterator.hpp
  PRIVATE
    FILE_SET state_db_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      merge_iterator.hpp
      state_delta.hpp
  PRIVATE
    state_db.cpp
    state_delta.cpp
    merge_iterator.cpp
    backends/backend.cpp
    backends/iterator.cpp
    backends/map/map_backend.cpp
    backends/map/map_iterator.cpp
    backends/rocksdb/rocksdb_backend.cpp
    backends/rocksdb/rocksdb_iterator.cpp
    backends/rocksdb/object_cache.cpp)

target_link_libraries(state_db
  PRIVATE
    Koinos::exception
    Koinos::proto
    Koinos::crypto
    RocksDB::rocksdb)

add_library(Koinos::state_db ALIAS state_db)

koinos_add_format(TARGET state_db)

