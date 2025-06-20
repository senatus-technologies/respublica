#pragma once

#include <cstddef>
#include <variant>
#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/protocol/account.hpp>
#include <koinos/protocol/program.hpp>

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
  program_input input;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & id;
    ar & input;
  }

  std::size_t size() const noexcept;
};

using operation = std::variant< upload_program, call_program >;

} // namespace koinos::protocol

template< typename T >
concept Operation = std::same_as< koinos::protocol::operation, T >;
