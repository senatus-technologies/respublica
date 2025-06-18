#pragma once

#include <expected>
#include <string>
#include <system_error>
#include <utility>

namespace koinos::controller {

enum class reversion_errc : int // NOLINT(performance-enum-size)
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

enum class controller_errc : int // NOLINT(performance-enum-size)
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

struct reversion_category final: std::error_category
{
  const char* name() const noexcept final
  {
    return "reversion";
  }

  std::string message( int condition ) const final
  {
    using namespace std::string_literals;
    switch( static_cast< reversion_errc >( condition ) )
    {
      case reversion_errc::ok:
        return "ok"s;
      case reversion_errc::failure:
        return "failure"s;
      case reversion_errc::invalid_contract:
        return "invalid contract"s;
      case reversion_errc::invalid_event_name:
        return "invalid event name"s;
      case reversion_errc::invalid_account:
        return "invalid account"s;
      case reversion_errc::insufficient_privileges:
        return "insufficient privileges"s;
      case reversion_errc::insufficient_resources:
        return "insufficient rc"s;
      case reversion_errc::unknown_operation:
        return "unknown operation"s;
      case reversion_errc::read_only_context:
        return "read only context"s;
      case reversion_errc::stack_overflow:
        return "stack overflow"s;
    }
    std::unreachable();
  }
};

struct controller_category final: std::error_category
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
    }
    std::unreachable();
  }
};

std::error_code make_error_code( reversion_errc e );
std::error_code make_error_code( controller_errc e );

template< typename T >
using result = std::expected< T, std::error_code >;

} // namespace koinos::controller

template<>
struct std::is_error_code_enum< koinos::controller::reversion_errc >: public std::true_type
{};

template<>
struct std::is_error_code_enum< koinos::controller::controller_errc >: public std::true_type
{};
