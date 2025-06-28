#pragma once

#include <string>

#include <koinos/program/error.hpp>
#include <koinos/program/program.hpp>

namespace koinos::program {

struct coin final: public program
{
  coin()              = default;
  coin( const coin& ) = delete;
  coin( coin&& )      = delete;
  ~coin() override    = default;

  coin& operator=( const coin& ) = delete;
  coin& operator=( coin&& )      = delete;

  std::error_code run( program_interface* system, std::span< const std::string > arguments ) override;

private:
  static constexpr std::string name       = "Coin";
  static constexpr std::string symbol     = "COIN";
  static constexpr std::uint32_t decimals = 8;

  static constexpr std::uint32_t supply_id  = 0;
  static constexpr std::uint32_t balance_id = 1;

  enum class instruction : std::uint32_t // NOLINT(performance-enum-size)
  {
    authorize,
    name,
    symbol,
    decimals,
    total_supply,
    balance_of,
    transfer,
    mint,
    burn
  };

  result< std::uint64_t > total_supply( program_interface* system );
  result< std::uint64_t > balance_of( program_interface* system, std::span< const std::byte > account );
};

} // namespace koinos::program
