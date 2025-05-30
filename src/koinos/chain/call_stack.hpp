#include <array>
#include <span>
#include <vector>

#include <koinos/chain/types.hpp>
#include <koinos/error/error.hpp>

namespace koinos::chain {

struct stack_frame
{
  std::span< const std::byte > contract_id;
  std::vector< std::vector< std::byte > > arguments;
  uint32_t entry_point = 0;
  std::vector< std::byte > output;
};

class call_stack
{
public:
  static constexpr std::size_t default_stack_limit = 32;

  call_stack( std::size_t stack_limit = default_stack_limit );

  error push_frame( stack_frame&& f );
  stack_frame& peek_frame();
  stack_frame pop_frame();
  std::size_t size() const;

private:
  std::vector< stack_frame > _stack;
  const std::size_t _limit;
};

} // namespace koinos::chain
