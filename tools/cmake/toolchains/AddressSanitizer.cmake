include(${CMAKE_CURRENT_LIST_DIR}/Clang.cmake)

add_compile_options(
  -fsanitize=address
  -fsanitize=undefined
  -fno-sanitize-recover=all)
add_link_options(
  -fsanitize=address
  -fsanitize=undefined
  -fno-sanitize-recover=all)
