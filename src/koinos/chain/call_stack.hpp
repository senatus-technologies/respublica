#include <span>
#include <system_error>
#include <vector>

namespace koinos::chain {

struct stack_frame
{
  std::span< const std::byte > program_id;
  std::span< const std::span< const std::byte > > arguments;
  std::vector< std::byte > output;
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

} // namespace koinos::chain
