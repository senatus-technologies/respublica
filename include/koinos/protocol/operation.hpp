#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/protocol/account.hpp>

namespace koinos::protocol {

struct upload_program
{
  account id{};
  std::vector< std::byte > bytecode;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & id;
    ar & bytecode;
  }

  std::size_t size() const noexcept;
};

struct call_program
{
  account id{};
  std::uint32_t entry_point = 0;
  std::vector< std::vector< std::byte > > arguments;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & id;
    ar & entry_point;
    ar & arguments;
  }

  std::size_t size() const noexcept;
};

using operation = std::variant< upload_program, call_program >;

} // namespace koinos::protocol

template< typename T >
concept Operation = std::same_as< koinos::protocol::operation, T >;
