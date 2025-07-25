#include <respublica/controller/frame_recorder.hpp>
#include <respublica/memory.hpp>

namespace respublica::controller {

void frame_recorder_session::add( std::shared_ptr< protocol::program_frame >& frame ) noexcept
{
  _frames.push_back( frame );
}

std::vector< std::shared_ptr< protocol::program_frame > >& frame_recorder_session::frames() noexcept
{
  return _frames;
}

void frame_recorder::set_session( const std::shared_ptr< frame_recorder_session >& s ) noexcept
{
  _session = s;
}

void frame_recorder::add( std::shared_ptr< protocol::program_frame >& frame ) noexcept
{
  if( auto session = _session.lock() )
    session->add( frame );

  _frames.push_back( frame );
}

std::vector< std::shared_ptr< protocol::program_frame > >& frame_recorder::frames() noexcept
{
  return _frames;
}

} // namespace respublica::controller
