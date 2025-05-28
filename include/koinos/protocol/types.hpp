#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

template< class T, class Archive >
concept Serializable = requires( T t, Archive a, const unsigned int v ) {
  { t.serialize( a, v ) } -> std::same_as< void >;
};

template< typename T >
concept Archivable = requires( boost::archive::binary_oarchive& oa, T a ) { oa << a; };

namespace koinos::protocol {

using account           = std::array< std::byte, 32 >;
using account_signature = std::array< std::byte, 64 >;

using digest = std::array< std::byte, 32 >;

struct event
{
  uint32_t sequence = 0;
  account source{};
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

constexpr inline account system_account( std::string_view str )
{
  account a{};
  std::size_t length = std::min( str.length(), a.size() );
  for( std::size_t i = 0; i < length; ++i )
    a[ i ] = static_cast< std::byte >( str[ i ] );
  return a;
}

} // namespace koinos::protocol

namespace std {
template<>
struct hash< koinos::protocol::account >
{
  size_t operator()( const koinos::protocol::account& arr ) const
  {
    size_t seed = 0;
    for( const auto& value: arr )
    {
      seed ^= std::hash< std::byte >()( value ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
    }
    return seed;
  }
};

} // namespace std
