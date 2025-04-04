add_library(chain)

target_sources(chain
  PUBLIC
    FILE_SET chain_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/include
    FILES
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/chronicler.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/constants.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/controller.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/exceptions.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/execution_context.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/host_api.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/indexer.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/resource_meter.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/session.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/state.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/system_calls.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/thunk_dispatcher.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/thunk_utils.hpp
      ${PROJECT_SOURCE_DIR}/include/koinos/chain/types.hpp
  PRIVATE
    FILE_SET chain_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}/src
    FILES
      proto_utils.hpp
      rectify.hpp
  PRIVATE
    chronicler.cpp
    controller.cpp
    execution_context.cpp
    host_api.cpp
    indexer.cpp
    proto_utils.cpp
    rectify.cpp
    resource_meter.cpp
    session.cpp
    state.cpp
    system_calls.cpp
    thunk_dispatcher.cpp)

target_link_libraries(chain
  PRIVATE
    Koinos::state_db
    Koinos::exception
    Koinos::crypto
    Koinos::log
    Koinos::util
    Koinos::mq
    vm_manager)

koinos_add_format(TARGET chain)

