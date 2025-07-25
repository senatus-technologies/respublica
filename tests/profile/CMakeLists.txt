if(FAST_MALLOC)
  add_executable(profile)

  target_sources(profile
    PRIVATE
      profile.cpp)

  target_link_libraries(profile
    PRIVATE
      Boost::thread
      Boost::program_options
      Boost::system
      gperftools::profiler
      gperftools::tcmalloc_and_profiler
      respublica::fixture)

  respublica_add_format(TARGET profile)
endif()
