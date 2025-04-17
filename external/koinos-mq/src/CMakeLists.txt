# libkoinos_mq

add_library(mq
  koinos/mq/client.cpp
  koinos/mq/message_broker.cpp
  koinos/mq/request_handler.cpp
  koinos/mq/retryer.cpp

  ${PROJECT_SOURCE_DIR}/include/koinos/mq/client.hpp
  ${PROJECT_SOURCE_DIR}/include/koinos/mq/exception.hpp
  ${PROJECT_SOURCE_DIR}/include/koinos/mq/message_broker.hpp
  ${PROJECT_SOURCE_DIR}/include/koinos/mq/request_handler.hpp
  ${PROJECT_SOURCE_DIR}/include/koinos/mq/retryer.hpp
  ${PROJECT_SOURCE_DIR}/include/koinos/mq/util.hpp
)

target_link_libraries(
  mq
    PUBLIC
      Boost::headers
      Koinos::log
      Koinos::protocol
      Koinos::util
#      Koinos::exception
    PRIVATE
      rabbitmq::rabbitmq-static)

target_include_directories(
  mq
    PUBLIC
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>)

add_library(Koinos::mq ALIAS mq)

