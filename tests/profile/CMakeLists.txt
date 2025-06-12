add_executable(profile)

target_sources(profile
  PRIVATE
    profile.cpp)

target_link_libraries(profile
  PRIVATE
    Boost::program_options
    Boost::system
    gperftools::profiler
    gperftools::tcmalloc_and_profiler
    Koinos::fixture)

koinos_add_format(TARGET profile)

