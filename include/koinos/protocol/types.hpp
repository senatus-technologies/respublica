#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

template< class T, class Archive >
concept Serializable = requires( T t, Archive a, const unsigned int v ) {
  { t.serialize( a, v ) } -> std::same_as< void >;
};

namespace koinos::protocol {

using account           = std::array< std::byte, 32 >;
using account_signature = std::array< std::byte, 64 >;

using digest = std::array< std::byte, 32 >;

struct log
{
  account source;
  std::string message;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & source;
    ar & message;
  }
};

struct event
{
  uint32_t sequence;
  account source;
  std::string name;
  std::vector< std::byte > data;
  std::vector< account > impacted;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & sequence;
    ar & source;
    ar & name;
    ar & data;
    ar & impacted;
  }
};

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

inline account account_from_name( std::string_view str )
{
  account a;
  std::size_t length = std::min( str.length(), a.size() );
  for( std::size_t i = 0; i < length; ++i )
    a[ i ] = static_cast< std::byte >( str[ i ] );
  return a;
}

} // namespace koinos::protocol
