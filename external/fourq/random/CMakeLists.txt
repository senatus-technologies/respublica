target_sources(fourq
  PRIVATE
    FILE_SET random_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES
      ${PROJECT_SOURCE_DIR}/random/random.h
  PRIVATE
    random.c)

