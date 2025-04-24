#include <koinos/error/error.hpp>
#include <koinos/chain/types.hpp>

namespace koinos::chain {

struct stack_frame
{
  bytes_s contract_id;
  std::vector< bytes_v > arguments;
  uint32_t entry_point = 0;
  bytes_v output;
};

class call_stack
{
public:
  static constexpr std::size_t default_stack_limit = 256;

  call_stack( std::size_t stack_limit = default_stack_limit );

  error push_frame( stack_frame&& f );
  stack_frame& peek_frame();
  stack_frame pop_frame();
  std::size_t size() const;

private:
  std::vector< stack_frame > _stack;
  const std::size_t _limit;
};

} // koinos::chain
