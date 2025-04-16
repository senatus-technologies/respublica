#include <koinos/error/error.hpp>

#include <string_view>

using namespace std::string_view_literals;

namespace koinos::error {

error::error( error_code ec ):
  _ec( ec )
{}

error::operator bool() const
{
  return _ec != error_code::success;
}

bool error::is_failure() const
{
  return _ec <= error_code::failure;
}

bool error::is_reversion() const
{
  return _ec >= error_code::reversion;
}

error_code error::value() const
{
  return _ec;
}

std::string_view error::message() const
{
  std::string_view error_message;

  switch( _ec )
  {
    case error_code::reversion:
      return "reversion"sv;
    case error_code::internal_error:
      return "internal error"sv;
    case error_code::invalid_contract:
      return "invalid contract"sv;
    case error_code::insufficient_rc:
      return "insufficient rc"sv;
    case error_code::insufficient_return_buffer:
      return "insufficient return buffer"sv;
    case error_code::unknown_operation:
      return "unknown operation"sv;
    case error_code::read_only_context:
      return "read only context"sv;
    case error_code::failure:
      return "failure"sv;
    case error_code::field_not_found:
      return "field not found"sv;
    case error_code::unknown_hash_code:
      return "unknown hash code"sv;
    case error_code::operation_not_found:
      return "operation not found"sv;
    case error_code::authorization_failure:
      return "authorization failure"sv;
    case error_code::malformed_block:
      return "malformed block"sv;
    case error_code::malformed_transaction:
      return "malformed transaction"sv;
    case error_code::block_resource_failure:
      return "block resource failure"sv;
    case error_code::pending_transaction_limit_exceeded:
      return "pending transaction limit exceeded"sv;
    case error_code::unknown_backend:
      return "unknown backed"sv;
    case error_code::unexpected_state:
      return "unexpected state"sv;
    case error_code::missing_required_arguments:
      return "missing required arguments"sv;
    case error_code::unknown_previous_block:
      return "unknown previous block"sv;
    case error_code::unexpected_height:
      return "unexpected height"sv;
    case error_code::block_state_error:
      return "block state error"sv;
    case error_code::state_merkle_mismatch:
      return "state merkle mismatch"sv;
    case error_code::unexpected_receipt:
      return "unexpected receipt"sv;
    case error_code::rpc_failure:
      return "rpc failure"sv;
    case error_code::pending_state_error:
      return "pending state error"sv;
    case error_code::timestamp_out_of_bounds:
      return "timestamp out of bounds"sv;
    case error_code::indexer_failure:
      return "indexer failure"sv;
    case error_code::network_bandwidth_limit_exceeded:
      return "network bandwidth limit exceeded"sv;
    case error_code::compute_bandwidth_limit_exceeded:
      return "compute bandwidth limit exceeded"sv;
    case error_code::disk_storage_limit_exceeded:
      return "disk storage limit exceeded"sv;
    case error_code::pre_irreversibility_block:
      return "pre irreversibility block"sv;
    default:
      return "unknown error"sv;
  }
}

} // namespace koinos::error
