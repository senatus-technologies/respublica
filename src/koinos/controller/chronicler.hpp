#pragma once
#include <koinos/protocol.hpp>

#include <memory>
#include <vector>

namespace koinos::controller {

class chronicler_session
{
public:
  protocol::program_output* add_frame( protocol::program_frame&& frame ) noexcept;
  std::vector< protocol::program_frame >& frames() noexcept;

private:
  std::vector< protocol::program_frame > _frames;
};

class chronicler final
{
public:
  void set_session( const std::shared_ptr< chronicler_session >& s ) noexcept;
  protocol::program_output* add_frame( protocol::program_frame&& frame ) noexcept;
  std::vector< protocol::program_frame >& frames() noexcept;

private:
  std::weak_ptr< chronicler_session > _session;
  std::vector< protocol::program_frame > _frames;
};

} // namespace koinos::controller
