#include <koinos/protocol/operation.hpp>

namespace koinos::protocol {

std::size_t upload_program::size() const noexcept
{
  std::size_t bytes = 0;

  bytes += id.size();
  bytes += bytecode.size();
  return bytes;
}

bool upload_program::validate() const noexcept
{
  return id.program();
}

std::size_t call_program::size() const noexcept
{
  std::size_t bytes = 0;

  bytes += id.size();

  for( const auto& argument: input.arguments )
    bytes += argument.size();

  bytes += input.stdin.size();

  return bytes;
}

bool call_program::validate() const noexcept
{
  return id.program();
}

} // namespace koinos::protocol
