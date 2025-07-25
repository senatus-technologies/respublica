#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>

#include <respublica/crypto.hpp>
#include <respublica/protocol/account.hpp>

namespace respublica::protocol {

struct event
{
  std::uint32_t sequence = 0;
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

} // namespace respublica::protocol
