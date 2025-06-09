include(${CMAKE_CURRENT_LIST_DIR}/Clang.cmake)

add_compile_options(
  -fsanitize=thread
  -g)
add_link_options(
  -fsanitize=thread)
