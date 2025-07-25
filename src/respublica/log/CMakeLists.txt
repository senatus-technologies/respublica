add_library(log)

target_sources(log
  PUBLIC
    FILE_SET log_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/respublica/log.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/log/formatter.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/log/frontend.hpp
      ${PROJECT_SOURCE_DIR}/include/respublica/log/log.hpp
  PRIVATE
    log.cpp)

target_link_libraries(log
  PUBLIC
    respublica::encode
    quill::quill
  PRIVATE
    respublica::memory)

target_compile_definitions(log
  PUBLIC
    QUILL_DISABLE_FUNCTION_NAME
    QUILL_IMMEDIATE_FLUSH=0
    $<$<CONFIG:Release>:QUILL_COMPILE_ACTIVE_LOG_LEVEL=QUILL_COMPILE_ACTIVE_LOG_LEVEL_INFO>)

add_library(respublica::log ALIAS log)

respublica_add_format(TARGET log)

