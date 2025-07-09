add_library(log)

target_sources(log
  PUBLIC
    FILE_SET log_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/log.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/log/formatter.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/log/frontend.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/log/log.hpp
  PRIVATE
    log.cpp)

target_link_libraries(log
  PUBLIC
    Koinos::encode
    quill::quill
  PRIVATE
    Koinos::memory)

target_compile_definitions(log
  PUBLIC
    QUILL_NO_EXCEPTIONS
    QUILL_DISABLE_FUNCTION_NAME
    QUILL_IMMEDIATE_FLUSH=0
    $<$<CONFIG:Release>:QUILL_COMPILE_ACTIVE_LOG_LEVEL=QUILL_COMPILE_ACTIVE_LOG_LEVEL_INFO>)

add_library(Koinos::log ALIAS log)

koinos_add_format(TARGET log)

