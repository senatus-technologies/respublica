#pragma once

#include <respublica/controller/frame_recorder.hpp>
#include <respublica/controller/resource_meter.hpp>

#include <cstdint>

namespace respublica::controller {

class session final: public resource_session,
                     public frame_recorder_session
{
public:
  session( std::uint64_t initial_resources );
};

} // namespace respublica::controller
