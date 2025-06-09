include(${CMAKE_CURRENT_LIST_DIR}/Clang.cmake)

add_compile_options(
  -fprofile-instr-generate
  -fcoverage-mapping)
add_link_options(
  -fprofile-instr-generate
  -fcoverage-mapping)
