include(${CMAKE_CURRENT_LIST_DIR}/Clang.cmake)

add_compile_options(
  -fsanitize=safe-stack)
add_link_options(
  -fsanitize=safe-stack)
