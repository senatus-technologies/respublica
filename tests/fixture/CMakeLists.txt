add_library(fixture)

target_sources(fixture
  PUBLIC
    FILE_SET fixture_headers
    TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    FILES
      ${CMAKE_CURRENT_SOURCE_DIR}/include/test/programs.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/test/fixture.hpp
      ${CMAKE_CURRENT_SOURCE_DIR}/include/test/wasm/koin.hpp
  PRIVATE
    programs.cpp
    fixture.cpp)

target_link_libraries(fixture
  PUBLIC
    Boost::filesystem
    Boost::system
    Koinos::controller
    Koinos::crypto
    Koinos::encode
    Koinos::log
    Koinos::protocol
  PRIVATE
    Koinos::state_db)

add_library(Koinos::fixture ALIAS fixture)

koinos_add_format(TARGET fixture)

