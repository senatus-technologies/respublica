#pragma once

#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/crypto.hpp>
#include <koinos/protocol/transaction.hpp>

namespace koinos::protocol {

struct program_output
{
  std::vector< std::byte > result;
  std::vector< std::string > logs;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & result;
    ar & logs;
  }
};

} // namespace koinos::protocol
