#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace koinos::vm {

enum class wasi_errno : std::uint16_t // NOLINT(performance-enum-size)
{
  success,
  e2big,
  acces,
  addrinuse,
  addrnotavail,
  afnosupport,
  again,
  already,
  badf,
  badmsg,
  busy,
  canceled,
  child,
  connaborted,
  connrefused,
  connreset,
  deadlk,
  destaddrreq,
  dom,
  dquot,
  exist,
  fault,
  fbig,
  hostunreach,
  idrm,
  ilseq,
  inprogress,
  intr,
  inval,
  io,
  isconn,
  isdir,
  loop,
  mfile,
  mlink,
  msgsize,
  multihop,
  nametoolong,
  netdown,
  netreset,
  netunreach,
  nfile,
  nobufs,
  nodev,
  noent,
  noexec,
  nolck,
  nolink,
  nomem,
  nomsg,
  noprotoopt,
  nospc,
  nosys,
  notconn,
  notdir,
  notempty,
  notrecoverable,
  notsock,
  notsup,
  notty,
  nxio,
  overflow,
  ownerdead,
  perm,
  pipe,
  proto,
  protonosupport,
  prototype,
  range,
  rofs,
  spipe,
  srch,
  stale,
  timedout,
  txtbsy,
  xdev,
  notcapable,
};

enum class wasi_fd_rights : std::uint32_t
{
  fd_datasync             = 1 << 0,
  fd_read                 = 1 << 1,
  fd_seek                 = 1 << 2,
  fd_fdstat_set_flags     = 1 << 3,
  fd_sync                 = 1 << 4,
  fd_tell                 = 1 << 5,
  fd_write                = 1 << 6,
  fd_advise               = 1 << 7,
  fd_allocate             = 1 << 8,
  path_create_directory   = 1 << 9,
  path_create_file        = 1 << 10,
  path_link_source        = 1 << 11,
  path_link_target        = 1 << 12,
  path_open               = 1 << 13,
  fd_readdir              = 1 << 14,
  path_readlink           = 1 << 15,
  path_rename_source      = 1 << 16,
  path_rename_target      = 1 << 17,
  path_filestat_get       = 1 << 18,
  path_filestat_set_size  = 1 << 19,
  path_filestat_set_times = 1 << 20,
  fd_filestat_get         = 1 << 21,
  fd_filestat_set_size    = 1 << 22,
  fd_filestat_set_times   = 1 << 23,
  path_symlink            = 1 << 24,
  path_remove_directory   = 1 << 25,
  path_unlink_file        = 1 << 26,
  poll_fd_readwrite       = 1 << 27,
  sock_shutdown           = 1 << 28,
  sock_accept             = 1 << 29
};

enum class wasi_fd : std::uint32_t // NOLINT(performance-enum-size)
{
  stdin,
  stdout,
  stderr
};

using io_vector = std::span< std::byte >;

/**
 * An abstract class representing an implementation of an API.
 *
 * The user of the vm_manager library is responsible for creating an application-specific subclass.
 */
class host_api
{
public:
  host_api()                  = default;
  host_api( const host_api& ) = delete;
  host_api( host_api&& )      = delete;
  virtual ~host_api()         = default;

  host_api& operator=( const host_api& ) = delete;
  host_api& operator=( host_api&& )      = delete;

  virtual std::int32_t wasi_args_get( std::uint32_t* argc, std::uint32_t* argv, char* argv_buf ) = 0;
  virtual std::int32_t wasi_args_sizes_get( std::uint32_t* argc, std::uint32_t* argv_buf_size )  = 0;
  virtual std::int32_t
  wasi_fd_seek( std::uint32_t fd, std::uint64_t offset, std::uint8_t* whence, std::uint8_t* newoffset ) = 0;
  virtual std::int32_t
  wasi_fd_write( std::uint32_t fd, const std::vector< io_vector > iovs, std::uint32_t* nwritten )               = 0;
  virtual std::int32_t wasi_fd_read( std::uint32_t fd, std::vector< io_vector > iovs, std::uint32_t* nwritten ) = 0;

  virtual std::int32_t wasi_fd_close( std::uint32_t fd )                              = 0;
  virtual std::int32_t wasi_fd_fdstat_get( std::uint32_t fd, std::uint32_t* buf_ptr ) = 0;
  virtual void wasi_proc_exit( std::int32_t exit_code )                               = 0;

  virtual std::int32_t koinos_get_caller( char* ret_ptr, std::uint32_t* ret_len )                                = 0;
  virtual std::int32_t koinos_get_object( std::uint32_t id,
                                          const char* key_ptr,
                                          std::uint32_t key_len,
                                          char* ret_ptr,
                                          std::uint32_t* ret_len )                                               = 0;
  virtual std::int32_t koinos_put_object( std::uint32_t id,
                                          const char* key_ptr,
                                          std::uint32_t key_len,
                                          const char* value_ptr,
                                          std::uint32_t value_len )                                              = 0;
  virtual std::int32_t koinos_check_authority( const char* account_ptr, std::uint32_t account_len, bool* value ) = 0;

  virtual std::uint64_t get_meter_ticks()                           = 0;
  virtual std::int32_t use_meter_ticks( std::uint64_t meter_ticks ) = 0;
};

} // namespace koinos::vm
