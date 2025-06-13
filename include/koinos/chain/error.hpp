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
  failure,
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
    switch( condition )
    {
      case 0:
        return "ok"s;
      case 1:
        return "temporary error, please retry"s;
      case 2:
        return "permanent error"s;
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
