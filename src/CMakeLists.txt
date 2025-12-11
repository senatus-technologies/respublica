add_subdirectory(respublica)

add_executable(net_client)

target_sources(net_client
  PRIVATE
    net_client.cpp)

target_link_libraries(net_client
  PRIVATE
    Boost::program_options
    respublica::net)

respublica_add_format(TARGET net_client)
