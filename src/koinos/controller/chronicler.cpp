#include <koinos/controller/chronicler.hpp>
#include <koinos/memory.hpp>

namespace koinos::controller {

protocol::program_output* chronicler_session::add_frame( protocol::program_frame&& frame ) noexcept
{
  return &*_frames.insert( _frames.end(), std::move( frame ) );
}

std::vector< protocol::program_frame >& chronicler_session::frames() noexcept
{
  return _frames;
}

void chronicler::set_session( const std::shared_ptr< chronicler_session >& s ) noexcept
{
  _session = s;
}

protocol::program_output* chronicler::add_frame( protocol::program_frame&& frame ) noexcept
{
  if( auto session = _session.lock() )
    return session->add_frame( std::move( frame ) );
  else
    return &*_frames.insert( _frames.end(), std::move( frame ) );
}

std::vector< protocol::program_frame >& chronicler::frames() noexcept
{
  return _frames;
}

} // namespace koinos::controller
