#pragma once

#include <koinos/controller/frame_recorder.hpp>
#include <koinos/controller/resource_meter.hpp>

#include <cstdint>

namespace koinos::controller {

class session final: public resource_session,
                     public frame_recorder_session
{
public:
  session( std::uint64_t initial_resources );
};

} // namespace koinos::controller
