#include <koinos/vm/error.hpp>
#include <string>
#include <system_error>
#include <utility>

namespace koinos::vm {

struct _virtual_machine_category final: std::error_category
{
  const char* name() const noexcept final
  {
    return "virtual machine";
  }

  std::string message( int condition ) const final
  {
    using namespace std::string_literals;
    switch( static_cast< virtual_machine_errc >( condition ) )
    {
      case virtual_machine_errc::ok:
        return "ok"s;
      case virtual_machine_errc::trapped:
        return "trapped"s;
      case virtual_machine_errc::invalid_arguments:
        return "invalid arguments"s;
      case virtual_machine_errc::execution_environment_failure:
        return "execution environment failure"s;
      case virtual_machine_errc::function_lookup_failure:
        return "function lookup failure"s;
      case virtual_machine_errc::load_failure:
        return "load failure"s;
      case virtual_machine_errc::instantiate_failure:
        return "instantiate failure"s;
      case virtual_machine_errc::invalid_pointer:
        return "invalid pointer"s;
      case virtual_machine_errc::invalid_module:
        return "invalid module"s;
      case virtual_machine_errc::invalid_context:
        return "invalid context"s;
      case virtual_machine_errc::entry_point_not_found:
        return "entry point not found"s;
    }
    std::unreachable();
  }
};

struct _program_category final: std::error_category
{
  const char* name() const noexcept final
  {
    return "virtual machine program";
  }

  std::string message( int condition ) const final
  {
    return "program exited with code " + std::to_string( condition );
  }
};

struct _wasi_category final: std::error_category
{
  const char* name() const noexcept final
  {
    return "wasi";
  }

  std::string message( int condition ) const final
  {
    using namespace std::string_literals;
    switch( static_cast< wasi_errc >( condition ) )
    {
      case wasi_errc::success:
        return "success"s;
      case wasi_errc::e2big:
        return "e2big"s;
      case wasi_errc::acces:
        return "acces"s;
      case wasi_errc::addrinuse:
        return "addrinuse"s;
      case wasi_errc::addrnotavail:
        return "addrnotavail"s;
      case wasi_errc::afnosupport:
        return "afnosupport"s;
      case wasi_errc::again:
        return "again"s;
      case wasi_errc::already:
        return "already"s;
      case wasi_errc::badf:
        return "badf"s;
      case wasi_errc::badmsg:
        return "badmsg"s;
      case wasi_errc::busy:
        return "busy"s;
      case wasi_errc::canceled:
        return "canceled"s;
      case wasi_errc::child:
        return "child"s;
      case wasi_errc::connaborted:
        return "connaborted"s;
      case wasi_errc::connrefused:
        return "connrefused"s;
      case wasi_errc::connreset:
        return "connreset"s;
      case wasi_errc::deadlk:
        return "deadlk"s;
      case wasi_errc::destaddrreq:
        return "destaddrreq"s;
      case wasi_errc::dom:
        return "dom"s;
      case wasi_errc::dquot:
        return "dquot"s;
      case wasi_errc::exist:
        return "exist"s;
      case wasi_errc::fault:
        return "fault"s;
      case wasi_errc::fbig:
        return "fbig"s;
      case wasi_errc::hostunreach:
        return "hostunreach"s;
      case wasi_errc::idrm:
        return "idrm"s;
      case wasi_errc::ilseq:
        return "ilseq"s;
      case wasi_errc::inprogress:
        return "inprogress"s;
      case wasi_errc::intr:
        return "intr"s;
      case wasi_errc::inval:
        return "inval"s;
      case wasi_errc::io:
        return "io"s;
      case wasi_errc::isconn:
        return "isconn"s;
      case wasi_errc::isdir:
        return "isdir"s;
      case wasi_errc::loop:
        return "loop"s;
      case wasi_errc::mfile:
        return "mfile"s;
      case wasi_errc::mlink:
        return "mlink"s;
      case wasi_errc::msgsize:
        return "msgsize"s;
      case wasi_errc::multihop:
        return "multihop"s;
      case wasi_errc::nametoolong:
        return "nametoolong"s;
      case wasi_errc::netdown:
        return "netdown"s;
      case wasi_errc::netreset:
        return "netreset"s;
      case wasi_errc::netunreach:
        return "netunreach"s;
      case wasi_errc::nfile:
        return "nfile"s;
      case wasi_errc::nobufs:
        return "nobufs"s;
      case wasi_errc::nodev:
        return "nodev"s;
      case wasi_errc::noent:
        return "noent"s;
      case wasi_errc::noexec:
        return "noexec"s;
      case wasi_errc::nolck:
        return "nolck"s;
      case wasi_errc::nolink:
        return "nolink"s;
      case wasi_errc::nomem:
        return "nomem"s;
      case wasi_errc::nomsg:
        return "nomsg"s;
      case wasi_errc::noprotoopt:
        return "noprotoopt"s;
      case wasi_errc::nospc:
        return "nospc"s;
      case wasi_errc::nosys:
        return "nosys"s;
      case wasi_errc::notconn:
        return "notconn"s;
      case wasi_errc::notdir:
        return "notdir"s;
      case wasi_errc::notempty:
        return "notempty"s;
      case wasi_errc::notrecoverable:
        return "notrecoverable"s;
      case wasi_errc::notsock:
        return "notsock"s;
      case wasi_errc::notsup:
        return "notsup"s;
      case wasi_errc::notty:
        return "notty"s;
      case wasi_errc::nxio:
        return "nxio"s;
      case wasi_errc::overflow:
        return "overflow"s;
      case wasi_errc::ownerdead:
        return "ownerdead"s;
      case wasi_errc::perm:
        return "perm"s;
      case wasi_errc::pipe:
        return "pipe"s;
      case wasi_errc::proto:
        return "proto"s;
      case wasi_errc::protonosupport:
        return "protonosupport"s;
      case wasi_errc::prototype:
        return "prototype"s;
      case wasi_errc::range:
        return "range"s;
      case wasi_errc::rofs:
        return "rofs"s;
      case wasi_errc::spipe:
        return "spipe"s;
      case wasi_errc::srch:
        return "srch"s;
      case wasi_errc::stale:
        return "stale"s;
      case wasi_errc::timedout:
        return "timedout"s;
      case wasi_errc::txtbsy:
        return "txtbsy"s;
      case wasi_errc::xdev:
        return "xdev"s;
      case wasi_errc::notcapable:
        return "notcapable"s;
    }
    std::unreachable();
  }
};

const std::error_category& virtual_machine_category() noexcept
{
  static _virtual_machine_category category;
  return category;
}

const std::error_category& program_category() noexcept
{
  static _program_category category;
  return category;
}

const std::error_category& wasi_category() noexcept
{
  static _wasi_category category;
  return category;
}

std::error_code make_error_code( virtual_machine_errc e )
{
  return std::error_code( static_cast< int >( e ), virtual_machine_category() );
}

std::error_code make_error_code( std::int32_t e )
{
  return std::error_code( static_cast< int >( e ), program_category() );
}

std::error_code make_error_code( wasi_errc e )
{
  return std::error_code( static_cast< int >( e ), wasi_category() );
}

} // namespace koinos::vm
