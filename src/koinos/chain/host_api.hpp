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

  void
  call( uint32_t sid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written );
  virtual int32_t invoke_thunk( uint32_t tid,
                                char* ret_ptr,
                                uint32_t ret_len,
                                const char* arg_ptr,
                                uint32_t arg_len,
                                uint32_t* bytes_written ) override;
  virtual int32_t invoke_system_call( uint32_t sid,
                                      char* ret_ptr,
                                      uint32_t ret_len,
                                      const char* arg_ptr,
                                      uint32_t arg_len,
                                      uint32_t* bytes_written ) override;

  virtual int32_t koinos_get_caller( char* ret_ptr, uint32_t* ret_len ) override;
  virtual int32_t koinos_get_object( uint32_t id, const char* key_ptr, uint32_t key_len, char* ret_ptr, uint32_t* ret_len ) override;
  virtual int32_t koinos_put_object( uint32_t id, const char* key_ptr, uint32_t key_len, const char* value_ptr, uint32_t value_len ) override;
  virtual int32_t koinos_check_authority( const char* account_ptr, uint32_t account_len, const char* data_ptr, uint32_t data_len, bool* value ) override;
  virtual int32_t koinos_log( const char* msg_ptr, uint32_t msg_len ) override;
  virtual int32_t koinos_exit( int32_t code, const char* res_bytes, uint32_t res_len ) override;
  virtual int32_t koinos_get_arguments( uint32_t* entry_point, char* args_ptr, uint32_t* args_len ) override;

  virtual int64_t get_meter_ticks() const override;
  virtual void use_meter_ticks( uint64_t meter_ticks ) override;
};

} // namespace koinos::chain
