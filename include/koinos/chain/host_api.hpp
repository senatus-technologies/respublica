#pragma once

#include <type_traits>

#include <koinos/chain/execution_context.hpp>
#include <koinos/chain/types.hpp>

namespace koinos::chain {

class host_api final: public vm_manager::abstract_host_api
{
public:
  host_api( execution_context& ctx );
  virtual ~host_api() override;

  execution_context& _ctx;

  virtual int32_t wasi_args_get(uint32_t* argc, uint32_t* argv, char* argv_buf ) override;
  virtual int32_t wasi_args_sizes_get( uint32_t* argc, uint32_t* argv_buf_size ) override;
  virtual int32_t wasi_fd_seek( uint32_t fd, uint64_t offset, uint8_t* whence, uint8_t* newoffset ) override;
  virtual int32_t wasi_fd_write( uint32_t fd, const uint8_t* iovs, uint32_t iovs_len, uint32_t* nwritten ) override;
  virtual int32_t wasi_fd_close( uint32_t fd ) override;
  virtual int32_t wasi_fd_fdstat_get( uint32_t fd, uint8_t* buf_ptr ) override;

  virtual int32_t koinos_get_caller( char* ret_ptr, uint32_t* ret_len ) override;
  virtual int32_t koinos_get_object( uint32_t id, const char* key_ptr, uint32_t key_len, char* ret_ptr, uint32_t* ret_len ) override;
  virtual int32_t koinos_put_object( uint32_t id, const char* key_ptr, uint32_t key_len, const char* value_ptr, uint32_t value_len ) override;
  virtual int32_t koinos_check_authority( const char* account_ptr, uint32_t account_len, const char* data_ptr, uint32_t data_len, bool* value ) override;
  virtual int32_t koinos_log( const char* msg_ptr, uint32_t msg_len ) override;
};

} // namespace koinos::chain
