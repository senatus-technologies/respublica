include(${CMAKE_CURRENT_LIST_DIR}/Clang.cmake)

find_program(CLANG_TIDY clang-tidy REQUIRED)

set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY};
  --warnings-as-errors=*;
  --allow-no-checks
  --header-filter=.*;
  --use-color)
set(CMAKE_C_CLANG_TIDY ${CLANG_TIDY};
  --warnings-as-errors=*;
  --allow-no-checks;
  --header-filter=.*;
  --use-color)
