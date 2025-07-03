#pragma once

#include <cstdint>
#include <expected>
#include <system_error>
#include <type_traits>

namespace koinos::vm {

enum class virtual_machine_errc : int // NOLINT(performance-enum-size)
{
  ok = 0,
  trapped,
  invalid_arguments,
  execution_environment_failure,
  function_lookup_failure,
  load_failure,
  instantiate_failure,
  invalid_pointer,
  invalid_module,
  invalid_context,
  entry_point_not_found
};

enum class wasi_errc : int // NOLINT(performance-enum-size)
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

const std::error_category& virtual_machine_category() noexcept;
const std::error_category& program_category() noexcept;
const std::error_category& wasi_category() noexcept;

std::error_code make_error_code( virtual_machine_errc e );
std::error_code make_error_code( std::int32_t e );
std::error_code make_error_code( wasi_errc e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace koinos::vm

template<>
struct std::is_error_code_enum< koinos::vm::virtual_machine_errc >: public std::true_type
{};

template<>
struct std::is_error_code_enum< koinos::vm::wasi_errc >: public std::true_type
{};
