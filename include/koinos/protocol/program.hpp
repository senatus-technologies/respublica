#pragma once

#include <string>
#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/protocol/account.hpp>

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
  std::int32_t code = 0;
  std::vector< std::byte > stdout;
  std::vector< std::byte > stderr;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & code;
    ar & stdout;
    ar & stderr;
  }
};

struct program_frame: program_output
{
  std::uint32_t depth = 0;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar& boost::serialization::base_object< program_output >( *this );
    ar & depth;
  }
};

} // namespace koinos::protocol
