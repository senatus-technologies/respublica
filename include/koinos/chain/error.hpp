#pragma once

#include <expected>
#include <string>
#include <system_error>
#include <utility>

namespace koinos::chain {

// NOLINTBEGIN(performance-enum-size)

enum class reversion_code : int
{
  ok = 0,
  failure,
  invalid_contract,
  invalid_event_name,
  invalid_account,
  insufficient_privileges,
  insufficient_resources,
  unknown_operation,
  read_only_context,
  stack_overflow
};

enum class controller_code : int
{
  ok = 0,
  authorization_failure,
  invalid_nonce,
  invalid_signature,
  malformed_block,
  malformed_transaction,
  insufficient_resources,
  unknown_previous_block,
  unexpected_height,
  block_state_error,
  state_merkle_mismatch,
  network_id_mismatch,
  timestamp_out_of_bounds,
  network_bandwidth_limit_exceeded,
  compute_bandwidth_limit_exceeded,
  disk_storage_limit_exceeded,
  pre_irreversibility_block
};

// NOLINTEND

struct reversion_category: std::error_category
{
  const char* name() const noexcept override
  {
    return "reversion";
  }

  std::string message( int condition ) const override
  {
    using namespace std::string_literals;
    switch( static_cast< reversion_code >( condition ) )
    {
      case reversion_code::ok:
        return "ok"s;
      case reversion_code::failure:
        return "failure"s;
      case reversion_code::invalid_contract:
        return "invalid contract"s;
      case reversion_code::invalid_event_name:
        return "invalid event name"s;
      case reversion_code::invalid_account:
        return "invalid account"s;
      case reversion_code::insufficient_privileges:
        return "insufficient privileges"s;
      case reversion_code::insufficient_resources:
        return "insufficient rc"s;
      case reversion_code::unknown_operation:
        return "unknown operation"s;
      case reversion_code::read_only_context:
        return "read only context"s;
      case reversion_code::stack_overflow:
        return "stack overflow"s;
    }
    std::unreachable();
  }
};

struct controller_category: std::error_category
{
  const char* name() const noexcept override
  {
    return "controller";
  }

  std::string message( int condition ) const override
  {
    using namespace std::string_literals;
    switch( static_cast< controller_code >( condition ) )
    {
      case controller_code::ok:
        return "ok"s;
      case controller_code::authorization_failure:
        return "authorization failure"s;
      case controller_code::invalid_nonce:
        return "invalid nonce"s;
      case controller_code::invalid_signature:
        return "invalid signature"s;
      case controller_code::malformed_block:
        return "malformed block"s;
      case controller_code::malformed_transaction:
        return "malformed transaction"s;
      case controller_code::insufficient_resources:
        return "insufficient resources"s;
      case controller_code::unknown_previous_block:
        return "unknown previous block"s;
      case controller_code::unexpected_height:
        return "unexpected height"s;
      case controller_code::block_state_error:
        return "block state error"s;
      case controller_code::state_merkle_mismatch:
        return "state merkle mismatch"s;
      case controller_code::network_id_mismatch:
        return "network id mismatch"s;
      case controller_code::timestamp_out_of_bounds:
        return "timestamp out of bounds"s;
      case controller_code::network_bandwidth_limit_exceeded:
        return "network bandwidth limit exceeded"s;
      case controller_code::compute_bandwidth_limit_exceeded:
        return "compute bandwidth limit exceeded"s;
      case controller_code::disk_storage_limit_exceeded:
        return "disk storage limit exceeded"s;
      case controller_code::pre_irreversibility_block:
        return "pre-irreversibility block"s;
    }
    std::unreachable();
  }
};

std::error_code make_error_code( koinos::chain::reversion_code e );
std::error_code make_error_code( koinos::chain::controller_code e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace koinos::chain

template<>
struct std::is_error_code_enum< koinos::chain::reversion_code >: public std::true_type
{};

template<>
struct std::is_error_code_enum< koinos::chain::controller_code >: public std::true_type
{};
