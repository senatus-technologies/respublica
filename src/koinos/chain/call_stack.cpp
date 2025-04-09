#include <koinos/chain/call_stack.hpp>

namespace koinos::chain {

call_stack::call_stack( std::size_t stack_limit ) :
  _limit( stack_limit )
{
  _stack.reserve( _limit );
}

error_code call_stack::push_frame( stack_frame&& f )
{
  KOINOS_CHECK_ERROR( _stack.size() < _limit, error_code::stack_overflow, "call stack overflow" );

  _stack.emplace_back( std::move( f ) );

  return {};
}

stack_frame& call_stack::peek_frame()
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "stack is empty" );

  return *_stack.rbegin();
}

void stack_frame call_stack::pop_frame()
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
