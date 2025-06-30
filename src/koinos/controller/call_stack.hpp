#include <cstddef>
#include <span>
#include <system_error>
#include <vector>

#include <koinos/protocol/program.hpp>

namespace koinos::controller {

struct stack_frame
{
  std::span< const std::byte > program_id;
  std::span< const std::string > arguments;
  std::span< const std::byte > stdin;
  std::vector< std::byte > stdout;
  std::vector< std::byte > stderr;

  std::size_t stdin_offset = 0;
};

class call_stack
{
public:
  static constexpr std::size_t default_stack_limit = 32;

  call_stack( std::size_t stack_limit = default_stack_limit );

  std::error_code push_frame( stack_frame&& f );
  stack_frame& peek_frame();
  stack_frame pop_frame();
  std::size_t size() const;

private:
  std::vector< stack_frame > _stack;
  std::size_t _limit;
};

} // namespace koinos::controller
