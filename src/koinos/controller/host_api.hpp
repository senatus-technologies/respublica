#pragma once

#include <koinos/controller/execution_context.hpp>

namespace koinos::controller {

class host_api final: public vm_manager::abstract_host_api
{
public:
  host_api() = delete;
  host_api( execution_context& ctx );
  host_api( const host_api& ) = delete;
  host_api( host_api&& )      = delete;
  ~host_api() final;

  host_api& operator=( const host_api& ) = delete;
  host_api& operator=( host_api&& )      = delete;

  std::int32_t wasi_args_get( std::uint32_t* argc, std::uint32_t* argv, char* argv_buf ) final;
  std::int32_t wasi_args_sizes_get( std::uint32_t* argc, std::uint32_t* argv_buf_size ) final;
  std::int32_t
  wasi_fd_seek( std::uint32_t fd, std::uint64_t offset, std::uint8_t* whence, std::uint8_t* newoffset ) final;
  std::int32_t
  wasi_fd_write( std::uint32_t fd, const std::uint8_t* iovs, std::uint32_t iovs_len, std::uint32_t* nwritten ) final;
  std::int32_t wasi_fd_close( std::uint32_t fd ) final;
  std::int32_t wasi_fd_fdstat_get( std::uint32_t fd, std::uint8_t* buf_ptr ) final;

  std::int32_t koinos_get_caller( char* ret_ptr, std::uint32_t* ret_len ) final;
  std::int32_t koinos_get_object( std::uint32_t id,
                                  const char* key_ptr,
                                  std::uint32_t key_len,
                                  char* ret_ptr,
                                  std::uint32_t* ret_len ) final;
  std::int32_t koinos_put_object( std::uint32_t id,
                                  const char* key_ptr,
                                  std::uint32_t key_len,
                                  const char* value_ptr,
                                  std::uint32_t value_len ) final;
  std::int32_t koinos_check_authority( const char* account_ptr,
                                       std::uint32_t account_len,
                                       const char* data_ptr,
                                       std::uint32_t data_len,
                                       bool* value ) final;
  std::int32_t koinos_log( const char* msg_ptr, std::uint32_t msg_len ) final;

private:
  execution_context& _ctx;
};

} // namespace koinos::controller
