#include <koinos/chain/call_stack.hpp>

#include <stdexcept>

using koinos::error::error_code;

namespace koinos::chain {

call_stack::call_stack( std::size_t stack_limit ):
    _limit( stack_limit )
{}

error call_stack::push_frame( stack_frame&& f )
{
  if( _stack.size() >= _limit )
    return { error_code::stack_overflow };

  _stack.emplace_back( std::move( f ) );

  return {};
}

stack_frame& call_stack::peek_frame()
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "stack is empty" );

  return *_stack.rbegin();
}

stack_frame call_stack::pop_frame()
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "stack is empty" );

  stack_frame frame = *_stack.rbegin();
  _stack.pop_back();

  return frame;
}

std::size_t call_stack::size() const
{
  return _stack.size();
}

} // namespace koinos::chain
