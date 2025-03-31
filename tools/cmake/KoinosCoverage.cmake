if(COVERAGE)
  add_compile_options(-g -fprofile-arcs -ftest-coverage)
endif()

macro(koinos_coverage)
  set(options)
  set(oneValueArgs EXECUTABLE)
  set(multiValueArgs EXCLUDE)
  cmake_parse_arguments(KOINOS_COVERAGE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(COVERAGE)
    include(CodeCoverage)
    append_coverage_compiler_flags()
    setup_target_for_coverage_lcov(
      NAME coverage
      LCOV_ARGS "--quiet" "--no-external"
      EXECUTABLE ${KOINOS_COVERAGE_EXECUTABLE}
      EXCLUDE ${KOINOS_COVERAGE_EXCLUDE})
  endif()
endmacro()
