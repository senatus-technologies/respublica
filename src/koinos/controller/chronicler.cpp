#include <koinos/controller/chronicler.hpp>
#include <koinos/memory.hpp>

namespace koinos::controller {

std::shared_ptr< protocol::program_output >
frame_recorder_session::add( std::shared_ptr< protocol::program_frame >& frame ) noexcept
{
  return *_frames.insert( _frames.end(), frame );
}

std::vector< std::shared_ptr< protocol::program_frame > >& frame_recorder_session::frames() noexcept
{
  return _frames;
}

void frame_recorder::set_session( const std::shared_ptr< frame_recorder_session >& s ) noexcept
{
  _session = s;
}

std::shared_ptr< protocol::program_output >
frame_recorder::add( std::shared_ptr< protocol::program_frame >& frame ) noexcept
{
  if( auto session = _session.lock() )
    return session->add( frame );
  else
    return *_frames.insert( _frames.end(), frame );
}

std::vector< std::shared_ptr< protocol::program_frame > >& frame_recorder::frames() noexcept
{
  return _frames;
}

} // namespace koinos::controller
