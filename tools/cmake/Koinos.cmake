list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

option(STATIC_ANALYSIS "Run static analysis during build" OFF)
option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)" OFF)
option(HUNTER_RUN_UPLOAD "Upload Hunter packages to binary cache server" OFF)
option(COVERAGE "Run code coverage" OFF)
option(BUILD_TESTING "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)

set(SANITIZER "None" CACHE STRING "Sanitizer build type, options are: None Address Stack Thread")
set_property(CACHE SANITIZER PROPERTY STRINGS None Address Stack Thread)

include(KoinosCompilerOptions)
include(KoinosStaticAnalysis)
include(KoinosCoverage)
include(KoinosUnitTests)
include(KoinosFormat)
include(KoinosAddPackage)
include(KoinosVersion)
include(KoinosGenerator)
include(KoinosGit)
include(KoinosInstall)

koinos_add_git_target()
