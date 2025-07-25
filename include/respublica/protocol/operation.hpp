#pragma once

#include <cstddef>
#include <variant>
#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/serialization/vector.hpp>

#include <respublica/protocol/account.hpp>
#include <respublica/protocol/program.hpp>

namespace respublica::protocol {

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
  bool validate() const noexcept;
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
  bool validate() const noexcept;
};

using operation = std::variant< upload_program, call_program >;

} // namespace respublica::protocol

template< typename T >
concept Operation = std::same_as< respublica::protocol::operation, T >;
