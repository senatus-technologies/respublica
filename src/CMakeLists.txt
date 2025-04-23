add_subdirectory(koinos)

add_executable(koinos_chain)

target_sources(koinos_chain
  PRIVATE
    koinos_chain.cpp)

target_link_libraries(koinos_chain
  PRIVATE
    Koinos::chain
    Koinos::git
    Koinos::mq
    Boost::program_options
    yaml-cpp::yaml-cpp)

target_compile_definitions(koinos_chain
  PRIVATE
    PROJECT_MAJOR_VERSION=${PROJECT_VERSION_MAJOR}
    PROJECT_MINOR_VERSION=${PROJECT_VERSION_MINOR}
    PROJECT_PATCH_VERSION=${PROJECT_VERSION_PATCH})

koinos_add_format(TARGET koinos_chain)

add_executable(koinos_vm_driver)

target_sources(koinos_vm_driver
  PRIVATE
   koinos_vm_driver.cpp)

target_link_libraries(koinos_vm_driver
  PRIVATE
    Koinos::chain
    Koinos::exception
    Koinos::protocol
    Koinos::util
    Boost::program_options)

koinos_add_format(TARGET koinos_vm_driver)

