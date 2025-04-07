add_subdirectory(koinos)

add_executable(koinos_chain)

target_sources(koinos_chain
  PRIVATE
    koinos_chain.cpp)

target_link_libraries(koinos_chain
  PRIVATE
    Koinos::chain
    Koinos::git
    Boost::program_options)

koinos_add_format(TARGET koinos_chain)

add_executable(koinos_vm_driver)

target_sources(koinos_vm_driver
  PRIVATE
   koinos_vm_driver.cpp)

target_link_libraries(koinos_vm_driver
  PRIVATE
    Koinos::chain
    Boost::program_options)

koinos_add_format(TARGET koinos_vm_driver)

koinos_install(TARGETS koinos_chain koinos_vm_driver)

