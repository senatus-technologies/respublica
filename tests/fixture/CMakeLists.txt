add_library(fixture)

target_sources(fixture
  PUBLIC
    FILE_SET fixture_headers
    TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
      ${CMAKE_CURRENT_SOURCE_DIR}/include/test/programs.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/test/fixture.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/test/wasm/token.hpp
  PRIVATE
    programs.cpp
    fixture.cpp)

target_link_libraries(fixture
  PUBLIC
    Boost::filesystem
    Boost::system
    respublica::controller
    respublica::crypto
    respublica::encode
    respublica::log
    respublica::protocol
  PRIVATE
    respublica::state_db)

add_library(respublica::fixture ALIAS fixture)

respublica_add_format(TARGET fixture)

