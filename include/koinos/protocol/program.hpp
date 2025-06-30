#pragma once

#include <string>
#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

namespace koinos::protocol {

struct program_input
{
  std::vector< std::string > arguments;
  std::vector< std::byte > stdin;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & arguments;
    ar & stdin;
  }
};

struct program_output
{
  std::vector< std::byte > stdout;
  std::vector< std::byte > stderr;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & stdout;
    ar & stderr;
  }
};

} // namespace koinos::protocol
