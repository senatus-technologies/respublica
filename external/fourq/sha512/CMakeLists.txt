target_sources(fourq
  PRIVATE
    FILE_SET sha512_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES
      ${PROJECT_SOURCE_DIR}/sha512/sha512.h
  PRIVATE
    sha512.c)

