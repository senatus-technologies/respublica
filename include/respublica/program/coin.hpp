#pragma once

#include <string>

#include <respublica/program/error.hpp>
#include <respublica/program/program.hpp>

namespace respublica::program {

struct coin final: public program
{
  coin()              = default;
  coin( const coin& ) = delete;
  coin( coin&& )      = delete;
  ~coin() override    = default;

  coin& operator=( const coin& ) = delete;
  coin& operator=( coin&& )      = delete;

  std::error_code run( system_interface* system, std::span< const std::string > arguments ) override;

private:
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

  std::uint64_t total_supply( system_interface* system );
  std::uint64_t balance_of( system_interface* system, std::span< const std::byte > account );
};

} // namespace respublica::program
