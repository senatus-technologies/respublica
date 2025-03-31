if (STATIC_ANALYSIS)
  find_program(CLANG_TIDY clang-tidy)

  if (NOT CLANG_TIDY)
    message(FATAL_ERROR "Static analysis was requested but 'clang-tidy' was not found")
  endif()

  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY};
    --warnings-as-errors=*;
    --checks=portability-*,clang-analyzer-*,clang-analyzer-cplusplus*;
    --header-filter=.*)
  set(CMAKE_C_CLANG_TIDY ${CLANG_TIDY};
    --warnings-as-errors=*;
    --checks=portability-*,clang-analyzer-*;
    --header-filter=.*)

endif()
