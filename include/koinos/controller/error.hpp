#pragma once

#include <expected>
#include <system_error>

namespace koinos::controller {

enum class reversion_errc : int // NOLINT(performance-enum-size)
{
  ok = 0,
  failure,
  invalid_program,
  invalid_event_name,
  invalid_account,
  insufficient_privileges,
  insufficient_resources,
  unknown_operation,
  read_only_context,
  stack_overflow,
  bad_file_descriptor
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

const std::error_category& reversion_category() noexcept;
const std::error_category& controller_category() noexcept;

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
