#pragma once

#include <koinos/chain/chronicler.hpp>
#include <koinos/chain/resource_meter.hpp>

#include <cstdint>

namespace koinos::chain {

class session: public rc_session,
               public chronicler_session
{
public:
  session( std::uint64_t initial_resources );
};

} // namespace koinos::chain
