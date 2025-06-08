#pragma once

#include <string_view>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/crypto/crypto.hpp>

namespace koinos::protocol {

using account = crypto::public_key_data;

struct authorization
{
  account signer{};
  crypto::signature signature{};

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & signer;
    ar & signature;
  }

  constexpr std::size_t size() const noexcept
  {
    std::size_t bytes = 0;

    bytes += signer.size();
    bytes += signature.size();

    return bytes;
  }
};

constexpr inline account system_account( std::string_view str ) noexcept
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
      seed ^= std::hash< std::byte >()( value );
    }
    return seed;
  }
};

} // namespace std
