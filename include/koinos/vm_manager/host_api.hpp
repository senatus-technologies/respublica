#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace koinos::vm_manager {

using io_vector = std::span< std::byte >;

/**
 * An abstract class representing an implementation of an API.
 *
 * The user of the vm_manager library is responsible for creating an application-specific subclass.
 */
class abstract_host_api
{
public:
  abstract_host_api()                           = default;
  abstract_host_api( const abstract_host_api& ) = delete;
  abstract_host_api( abstract_host_api&& )      = delete;
  virtual ~abstract_host_api()                  = default;

  abstract_host_api& operator=( const abstract_host_api& ) = delete;
  abstract_host_api& operator=( abstract_host_api&& )      = delete;

  virtual std::int32_t wasi_args_get( std::uint32_t* argc, std::uint32_t* argv, char* argv_buf ) = 0;
  virtual std::int32_t wasi_args_sizes_get( std::uint32_t* argc, std::uint32_t* argv_buf_size )  = 0;
  virtual std::int32_t
  wasi_fd_seek( std::uint32_t fd, std::uint64_t offset, std::uint8_t* whence, std::uint8_t* newoffset ) = 0;
  virtual std::int32_t
  wasi_fd_write( std::uint32_t fd, const std::vector< io_vector > iovss, std::uint32_t* nwritten )               = 0;
  virtual std::int32_t wasi_fd_read( std::uint32_t fd, std::vector< io_vector > iovss, std::uint32_t* nwritten ) = 0;

  virtual std::int32_t wasi_fd_close( std::uint32_t fd )                              = 0;
  virtual std::int32_t wasi_fd_fdstat_get( std::uint32_t fd, std::uint32_t* buf_ptr ) = 0;
  virtual void wasi_proc_exit( std::int32_t exit_code )                               = 0;

  virtual std::int32_t koinos_get_caller( char* ret_ptr, std::uint32_t* ret_len ) = 0;
  virtual std::int32_t koinos_get_object( std::uint32_t id,
                                          const char* key_ptr,
                                          std::uint32_t key_len,
                                          char* ret_ptr,
                                          std::uint32_t* ret_len )                = 0;
  virtual std::int32_t koinos_put_object( std::uint32_t id,
                                          const char* key_ptr,
                                          std::uint32_t key_len,
                                          const char* value_ptr,
                                          std::uint32_t value_len )               = 0;
  virtual std::int32_t koinos_check_authority( const char* account_ptr,
                                               std::uint32_t account_len,
                                               const char* data_ptr,
                                               std::uint32_t data_len,
                                               bool* value )                      = 0;
  virtual std::int32_t koinos_log( const char* msg_ptr, std::uint32_t msg_len )   = 0;
};

} // namespace koinos::vm_manager
