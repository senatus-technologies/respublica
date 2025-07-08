#pragma once
#include <koinos/protocol.hpp>

#include <memory>
#include <vector>

namespace koinos::controller {

class frame_recorder_session
{
public:
  void add( std::shared_ptr< protocol::program_frame >& frame ) noexcept;
  std::vector< std::shared_ptr< protocol::program_frame > >& frames() noexcept;

private:
  std::vector< std::shared_ptr< protocol::program_frame > > _frames;
};

class frame_recorder final
{
public:
  void set_session( const std::shared_ptr< frame_recorder_session >& s ) noexcept;
  void add( std::shared_ptr< protocol::program_frame >& frame ) noexcept;
  std::vector< std::shared_ptr< protocol::program_frame > >& frames() noexcept;

private:
  std::weak_ptr< frame_recorder_session > _session;
  std::vector< std::shared_ptr< protocol::program_frame > > _frames;
};

} // namespace koinos::controller
