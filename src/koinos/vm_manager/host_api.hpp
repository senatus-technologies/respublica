#pragma once

#include <cstdint>

namespace koinos::vm_manager {

/**
 * An abstract class representing an implementation of an API.
 *
 * The user of the vm_manager library is responsible for creating an application-specific subclass.
 */
class abstract_host_api
{
   public:
      abstract_host_api();
      virtual ~abstract_host_api();

      virtual int32_t invoke_thunk( uint32_t tid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written  ) = 0;
      virtual int32_t invoke_system_call( uint32_t xid, char* ret_ptr, uint32_t ret_len, const char* arg_ptr, uint32_t arg_len, uint32_t* bytes_written  ) = 0;

      virtual int32_t koinos_get_caller( char* ret_ptr, uint32_t* ret_len ) = 0;
      virtual int32_t koinos_get_object( uint32_t id, const char* key_ptr, uint32_t key_len, char* ret_ptr, uint32_t* ret_len ) = 0;
      virtual int32_t koinos_put_object( uint32_t id, const char* key_ptr, uint32_t key_len, const char* value_ptr, uint32_t value_len ) = 0;
      virtual int32_t koinos_check_authority( const char* account_ptr, uint32_t account_len, const char* data_ptr, uint32_t data_len, bool* value ) = 0;
      virtual int32_t koinos_log( const char* msg_ptr, uint32_t msg_len ) = 0;
      virtual int32_t koinos_exit( int32_t code, const char* res_bytes, uint32_t res_len ) = 0;
      virtual int32_t koinos_get_arguments( uint32_t* entry_point, char* args_ptr, uint32_t* args_len ) = 0;

      virtual int64_t get_meter_ticks()const = 0;
      virtual void use_meter_ticks( uint64_t meter_ticks ) = 0;
};

} // koinos::vm_manager
