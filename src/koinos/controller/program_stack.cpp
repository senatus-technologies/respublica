#include <koinos/controller/error.hpp>
#include <koinos/controller/program_stack.hpp>

namespace koinos::controller {

program_stack::program_stack( std::size_t stack_limit ):
    _stack(),
    _limit( stack_limit )
{}

std::error_code program_stack::push_frame( stack_frame&& f ) noexcept
{
  if( _stack.size() >= _limit )
    return controller_errc::stack_overflow;

  _stack.emplace_back( std::move( f ) );

  return controller_errc::ok;
}

stack_frame& program_stack::peek_frame()
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "stack is empty" );

  return *_stack.rbegin();
}

stack_frame program_stack::pop_frame()
{
  if( _stack.size() == 0 )
    throw std::runtime_error( "stack is empty" );

  stack_frame frame = *_stack.rbegin();
  _stack.pop_back();

  return frame;
}

std::size_t program_stack::size() const
{
  return _stack.size();
}

} // namespace koinos::controller
