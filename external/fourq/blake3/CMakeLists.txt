target_sources(fourq
  PRIVATE
    FILE_SET blake3_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES
      ${PROJECT_SOURCE_DIR}/blake3/blake3.h
  PRIVATE
    blake3.c)

target_link_libraries(fourq PRIVATE BLAKE3::blake3)

