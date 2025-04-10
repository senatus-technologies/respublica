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

      virtual error_code wasi_args_get(uint32_t* argc, uint32_t* argv, char* argv_buf ) = 0;
      virtual error_code wasi_args_sizes_get( uint32_t* argc, uint32_t* argv_buf_size ) = 0;
      virtual error_code wasi_fd_seek( uint32_t fd, uint64_t offset, uint8_t* whence, uint8_t* newoffset ) = 0;
      virtual error_code wasi_fd_write( uint32_t fd, const uint8_t* iovs, uint32_t iovs_len, uint32_t* nwritten ) = 0;
      virtual error_code wasi_fd_close( uint32_t fd ) = 0;
      virtual error_code wasi_fd_fdstat_get( uint32_t fd, uint8_t* buf_ptr ) = 0;

      virtual error_code koinos_get_caller( char* ret_ptr, uint32_t* ret_len ) = 0;
      virtual error_code koinos_get_object( uint32_t id, const char* key_ptr, uint32_t key_len, char* ret_ptr, uint32_t* ret_len ) = 0;
      virtual error_code koinos_put_object( uint32_t id, const char* key_ptr, uint32_t key_len, const char* value_ptr, uint32_t value_len ) = 0;
      virtual error_code koinos_check_authority( const char* account_ptr, uint32_t account_len, const char* data_ptr, uint32_t data_len, bool* value ) = 0;
      virtual error_code koinos_log( const char* msg_ptr, uint32_t msg_len ) = 0;
      virtual error_code koinos_exit( int32_t code, const char* res_bytes, uint32_t res_len ) = 0;
      virtual error_code koinos_get_arguments( uint32_t* entry_point, char* args_ptr, uint32_t* args_len ) = 0;
};

} // koinos::vm_manager

