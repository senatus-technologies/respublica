#include <koinos/chain/errors.hpp>

#include <koinos/chain/error.pb.h>

namespace koinos::chain {

using namespace std::string_view_lirerals;

constexpr error_category_name = "error_code"sv;

error_code::error_code( int ec ) :
    std::error_code( ec, _category )
{}

error_code::error_code( int ec, std::string_view msg ) :
    std::error_code( ec, error_category( msg ) )
{}

bool error_code::is_failure() const
{
  return ec.value() <= error_code::failure;
}

bool error_code::is_reversion() const
{
  return ec.value() >= error_code::reversion;
}

error_category::error_category( std::string_view message ):
  _message( message )
{}

const char* error_category::name() const noexcept
{
  return error_category_name.data();
}

std::string error_category::message( int ev ) const
{
  std::string_view error_message;

  switch( static_cast< error_code >( int ) )
  {
    case error_code::reversion:
      error_message = "reversion"sv;
      break;
    case error_code::internal_error:
      error_message = "internal error"sv;
      break;
    case error_code::invalid_contract:
      error_message = "invalid contract"sv;
      break;
    case error_code::insufficient_rc:
      error_message = "insufficient rc"sv;
      break;
    case error_code::insufficient_return_buffer:
      error_message = "insufficient return buffer"sv;
      break;
    case error_code::unknown_operation:
      error_message = "unknown operation"sv;
      break;
    case error_code::read_only_context:
      error_message = "read only context"sv;
      break;
    case error_code::failure:
      error_message = "failure"sv;
      break;
    case error_code::field_not_found:
      error_message = "field not found"sv;
      break;
    case error_code::unknown_hash_code:
      error_message = "unknown hash code"sv;
      break;
    case error_code::operation_not_found:
      error_message = "operation not found"sv;
      break;
    case error_code::authorization_failure:
      error_message = "authorization failure"sv;
      break;
    case error_code::malformed_block:
      error_message = "malformed block"sv;
      break;
    case error_code::malformed_transaction:
      error_message = "malformed transaction"sv;
      break;
    case error_code::block_resource_failure:
      error_message = "block resource failure"sv;
      break;
    case error_code::pending_transaction_limit_exceeded:
      error_message = "pending transaction limit exceeded"sv;
      break;
    case error_code::unknown_backend:
      error_message = "unknown backed"sv;
      break;
    case error_code::unexpected_state:
      error_message = "unexpected state"sv;
      break;
    case error_code::missing_required_arguments:
      error_message = "missing required arguments"sv;
      break;
    case error_code::unknown_previous_block:
      error_message = "unknown previous block"sv;
      break;
    case error_code::unexpected_height:
      error_message = "unexpected height"sv;
      break;
    case error_code::block_state_error:
      error_message = "block state error"sv;
      break;
    case error_code::state_merkle_mismatch:
      error_message = "state merkle mismatch"sv;
      break;
    case error_code::unexpected_receipt:
      error_message = "unexpected receipt"sv;
      break;
    case error_code::rpc_failure:
      error_message = "rpc failure"sv;
      break;
    case error_code::pending_state_error:
      error_message = "pending state error"sv;
      break;
    case error_code::timestamp_out_of_bounds:
      error_message = "timestamp out of bounds"sv;
      break;
    case error_code::indexer_failure:
      error_message = "indexer failure"sv;
      break;
    case error_code::network_bandwidth_limit_exceeded:
      error_message = "network bandwidth limit exceeded"sv;
      break;
    case error_code::compute_bandwidth_limit_exceeded:
      error_message = "compute bandwidth limit exceeded"sv;
      break;
    case error_code::disk_storage_limit_exceeded:
      error_message = "disk storage limit exceeded"sv;
      break;
    case error_code::pre_irreversibility_block:
      error_message = "pre irreversibility block"sv;
      break;
    default:
      error_message = "unknown error"sv;
  }

  if( _message.size() )
    error_message += ', 'sv + _message;

  return error_message;
}

} // namespace koinos::chain
