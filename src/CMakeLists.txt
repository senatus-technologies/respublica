add_subdirectory(koinos)

add_executable(koinos_chain)

target_sources(koinos_chain
  PRIVATE
    koinos_chain.cpp)

target_link_libraries(koinos_chain
  PRIVATE
    Koinos::chain
    Koinos::git
    Boost::program_options
    yaml-cpp::yaml-cpp)

target_compile_definitions(koinos_chain
  PRIVATE
    PROJECT_MAJOR_VERSION=${PROJECT_VERSION_MAJOR}
    PROJECT_MINOR_VERSION=${PROJECT_VERSION_MINOR}
    PROJECT_PATCH_VERSION=${PROJECT_VERSION_PATCH})

koinos_add_format(TARGET koinos_chain)

