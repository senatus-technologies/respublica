#pragma once

#include <koinos/controller/chronicler.hpp>
#include <koinos/controller/resource_meter.hpp>

#include <cstdint>

namespace koinos::controller {

class session final: public resource_session,
               public chronicler_session
{
public:
  session( std::uint64_t initial_resources );
};

} // namespace koinos::controller
