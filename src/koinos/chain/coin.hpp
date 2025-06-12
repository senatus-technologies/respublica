#pragma once

#include <expected>
#include <koinos/chain/program.hpp>
#include <string>

namespace koinos::chain {

struct coin final: public program
{
  coin() = default;
  coin( const coin& ) = delete;
  coin( coin&& ) = delete;
  ~coin() override = default;

  coin& operator =( const coin& ) = delete;
  coin& operator =( coin&& ) = delete;

  error start( system_interface* system, uint32_t entry_point, std::span< const std::span< const std::byte > >& args ) override;

  std::expected< uint64_t, error > total_supply( system_interface* system );
  std::expected< uint64_t, error > balance_of( system_interface* system, std::span< const std::byte > account );

private:
  static constexpr std::string name   = "Coin";
  static constexpr std::string symbol = "COIN";
  static constexpr uint64_t decimals  = 8;

  static constexpr uint32_t supply_id  = 0;
  static constexpr uint32_t balance_id = 1;

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

} // namespace koinos::chain
