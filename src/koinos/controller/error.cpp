#include <koinos/controller/error.hpp>
#include <string>
#include <utility>

namespace koinos::controller {

struct _controller_category final: std::error_category
{
  const char* name() const noexcept final
  {
    return "controller";
  }

  std::string message( int condition ) const final
  {
    using namespace std::string_literals;
    switch( static_cast< controller_errc >( condition ) )
    {
      case controller_errc::ok:
        return "ok"s;
      case controller_errc::authorization_failure:
        return "authorization failure"s;
      case controller_errc::invalid_nonce:
        return "invalid nonce"s;
      case controller_errc::invalid_signature:
        return "invalid signature"s;
      case controller_errc::malformed_block:
        return "malformed block"s;
      case controller_errc::malformed_transaction:
        return "malformed transaction"s;
      case controller_errc::insufficient_resources:
        return "insufficient resources"s;
      case controller_errc::unknown_previous_block:
        return "unknown previous block"s;
      case controller_errc::unexpected_height:
        return "unexpected height"s;
      case controller_errc::block_state_error:
        return "block state error"s;
      case controller_errc::state_merkle_mismatch:
        return "state merkle mismatch"s;
      case controller_errc::network_id_mismatch:
        return "network id mismatch"s;
      case controller_errc::timestamp_out_of_bounds:
        return "timestamp out of bounds"s;
      case controller_errc::network_bandwidth_limit_exceeded:
        return "network bandwidth limit exceeded"s;
      case controller_errc::compute_bandwidth_limit_exceeded:
        return "compute bandwidth limit exceeded"s;
      case controller_errc::disk_storage_limit_exceeded:
        return "disk storage limit exceeded"s;
      case controller_errc::pre_irreversibility_block:
        return "pre-irreversibility block"s;
      case controller_errc::invalid_program:
        return "invalid program"s;
      case controller_errc::invalid_event_name:
        return "invalid event name"s;
      case controller_errc::invalid_account:
        return "invalid account"s;
      case controller_errc::insufficient_privileges:
        return "insufficient privileges"s;
      case controller_errc::unknown_operation:
        return "unknown operation"s;
      case controller_errc::read_only_context:
        return "read only context"s;
      case controller_errc::stack_overflow:
        return "stack overflow"s;
      case controller_errc::bad_file_descriptor:
        return "bad file descriptor"s;
      case controller_errc::unexpected_object:
        return "unexpected object"s;
      case controller_errc::insufficient_space:
        return "insufficient space"s;
    }
    std::unreachable();
  }
};

const std::error_category& controller_category() noexcept
{
  static _controller_category category;
  return category;
}

std::error_code make_error_code( controller_errc e )
{
  return std::error_code( static_cast< int >( e ), controller_category() );
}

} // namespace koinos::controller
