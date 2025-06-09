#include <koinos/protocol/operation.hpp>

namespace koinos::protocol {

std::size_t upload_program::size() const noexcept
{
  std::size_t bytes = 0;

  bytes += id.size();
  bytes += bytecode.size();
  return bytes;
}

std::size_t call_program::size() const noexcept
{
  std::size_t bytes = 0;

  bytes += id.size();
  bytes += sizeof( entry_point );

  for( const auto& argument: arguments )
    bytes += argument.size();

  return bytes;
}

} // namespace koinos::protocol
