#include <cstddef>
#include <span>
#include <system_error>
#include <vector>

#include <koinos/controller/error.hpp>
#include <koinos/protocol/program.hpp>

namespace koinos::controller {

struct stack_frame final
{
  std::span< const std::byte > program_id;
  std::span< const std::string > arguments;
  std::span< const std::byte > stdin;
  std::vector< std::byte > stdout;
  std::vector< std::byte > stderr;

  std::size_t stdin_offset = 0;
};

class call_stack final
{
public:
  static constexpr std::size_t default_stack_limit = 32;

  call_stack( std::size_t stack_limit = default_stack_limit );

  std::error_code push_frame( stack_frame&& f ) noexcept;
  stack_frame& peek_frame();
  stack_frame pop_frame();
  std::size_t size() const;

private:
  std::vector< stack_frame > _stack;
  std::size_t _limit;
};

struct frame_guard final
{
  frame_guard( const frame_guard& )            = delete;
  frame_guard( frame_guard&& )                 = delete;
  frame_guard& operator=( const frame_guard& ) = delete;
  frame_guard& operator=( frame_guard&& )      = delete;

  frame_guard( call_stack& stack ):
      _call_stack( &stack )
  {}

  ~frame_guard()
  {
    _call_stack->pop_frame();
  }

private:
  call_stack* _call_stack;
};

} // namespace koinos::controller
