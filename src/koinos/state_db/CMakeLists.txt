add_library(state_db)

target_sources(state_db
  PUBLIC
    FILE_SET state_db_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/state_db_types.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/state_db/state_db.hpp
  PRIVATE
    FILE_SET state_db_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      state_delta.hpp
      backends/backend.hpp
      backends/iterator.hpp
      backends/types.hpp
      backends/map/map_backend.hpp
      backends/map/map_iterator.hpp
#      backends/rocksdb/object_cache.hpp
#      backends/rocksdb/rocksdb_backend.hpp
#      backends/rocksdb/rocksdb_iterator.hpp
  PRIVATE
    state_db.cpp
    state_delta.cpp
    backends/backend.cpp
    backends/iterator.cpp
    backends/map/map_backend.cpp
    backends/map/map_iterator.cpp)
#    backends/rocksdb/rocksdb_backend.cpp
#    backends/rocksdb/rocksdb_iterator.cpp
#    backends/rocksdb/object_cache.cpp)

target_link_libraries(state_db
  PRIVATE
    Koinos::util
    Koinos::protocol
    Koinos::crypto
    RocksDB::rocksdb)

add_library(Koinos::state_db ALIAS state_db)

koinos_add_format(TARGET state_db)

