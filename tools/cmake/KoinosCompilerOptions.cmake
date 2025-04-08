set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(Boost_NO_BOOST_CMAKE ON)

# This is to force color output when using ccache with Unix Makefiles
if (FORCE_COLORED_OUTPUT)
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
    add_compile_options(-fdiagnostics-color=always)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" )
    add_compile_options(-fcolor-diagnostics)
  endif()
endif()

if (MSVC)
  # warning level 4 and all warnings as errors
  add_compile_options(/W4 /WX)
else()
  # lots of warnings and all warnings as errors
  add_compile_options(-Werror -Wno-unknown-pragmas)
endif()

if (SANITIZER MATCHES "Address")
  message(STATUS "Sanitizer configuration type: ${SANITIZER}")
  add_compile_options(
    -fsanitize=address
    -fsanitize=undefined
    -fno-sanitize-recover=all)
  add_link_options(
    -fsanitize=address
    -fsanitize=undefined
    -fno-sanitize-recover=all)
elseif(SANITIZER MATCHES "Stack")
  message(STATUS "Sanitizer configuration type: ${SANITIZER}")
  add_compile_options(
    -fsanitize=safe-stack)
  add_link_options(
    -fsanitize=safe-stack)
elseif(SANITIZER MATCHES "Thread")
  message(STATUS "Sanitizer configuration type: ${SANITIZER}")
  add_compile_options(
    -fsanitize=thread)
  add_link_options(
    -fsanitize=thread)
elseif(NOT SANITIZER MATCHES "None")
  message(FATAL_ERROR "Unknown value for option SANITIZER: '${SANITIZER}'. Must be None, Address, Stack, or Thread.")
endif()
