#pragma once

#include <koinos/chain/execution_context.hpp>

namespace koinos::chain {

class host_api final: public vm_manager::abstract_host_api
{
public:
  host_api() = delete;
  host_api( execution_context& ctx );
  host_api( const host_api& ) = delete;
  host_api( host_api&& ) = delete;
  ~host_api() override;

  host_api& operator =( const host_api& ) = delete;
  host_api& operator =( host_api&& ) = delete;

  int32_t wasi_args_get( uint32_t* argc, uint32_t* argv, char* argv_buf ) override;
  int32_t wasi_args_sizes_get( uint32_t* argc, uint32_t* argv_buf_size ) override;
  int32_t wasi_fd_seek( uint32_t fd, uint64_t offset, uint8_t* whence, uint8_t* newoffset ) override;
  int32_t wasi_fd_write( uint32_t fd, const uint8_t* iovs, uint32_t iovs_len, uint32_t* nwritten ) override;
  int32_t wasi_fd_close( uint32_t fd ) override;
  int32_t wasi_fd_fdstat_get( uint32_t fd, uint8_t* buf_ptr ) override;

  int32_t koinos_get_caller( char* ret_ptr, uint32_t* ret_len ) override;
  int32_t
  koinos_get_object( uint32_t id, const char* key_ptr, uint32_t key_len, char* ret_ptr, uint32_t* ret_len ) override;
  int32_t koinos_put_object( uint32_t id,
                                     const char* key_ptr,
                                     uint32_t key_len,
                                     const char* value_ptr,
                                     uint32_t value_len ) override;
  int32_t koinos_check_authority( const char* account_ptr,
                                          uint32_t account_len,
                                          const char* data_ptr,
                                          uint32_t data_len,
                                          bool* value ) override;
  int32_t koinos_log( const char* msg_ptr, uint32_t msg_len ) override;

private:
    execution_context& _ctx;
};

} // namespace koinos::chain
