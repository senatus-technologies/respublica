#pragma once

#include <string>

#include <koinos/controller/error.hpp>
#include <koinos/controller/program.hpp>

namespace koinos::controller {

struct coin final: public program
{
  coin()              = default;
  coin( const coin& ) = delete;
  coin( coin&& )      = delete;
  ~coin() override    = default;

  coin& operator=( const coin& ) = delete;
  coin& operator=( coin&& )      = delete;

  std::error_code start( system_interface* system, std::span< const std::string > arguments ) override;

  result< std::uint64_t > total_supply( system_interface* system );
  result< std::uint64_t > balance_of( system_interface* system, std::span< const std::byte > account );

private:
  static constexpr std::string name       = "Coin";
  static constexpr std::string symbol     = "COIN";
  static constexpr std::uint64_t decimals = 8;

  static constexpr std::uint32_t supply_id  = 0;
  static constexpr std::uint32_t balance_id = 1;

  enum entries
  {
    name_entry         = 0x82a3537f,
    symbol_entry       = 0xb76a7ca1,
    decimals_entry     = 0xee80fd2f,
    total_supply_entry = 0xb0da3934,
    balance_of_entry   = 0x5c721497,
    transfer_entry     = 0x27f576ca,
    mint_entry         = 0xdc6f17bb,
    burn_entry         = 0x859facc5,
  };
};

} // namespace koinos::controller
