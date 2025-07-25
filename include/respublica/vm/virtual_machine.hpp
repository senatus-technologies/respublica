#pragma once

#include <respublica/vm/host_api.hpp>

#include <memory>
#include <span>
#include <system_error>

namespace respublica::vm {

class module_cache;

class virtual_machine final
{
public:
  virtual_machine();
  virtual_machine( const virtual_machine& ) = delete;
  virtual_machine( virtual_machine&& )      = delete;
  ~virtual_machine();

  virtual_machine& operator=( const virtual_machine& ) = delete;
  virtual_machine& operator=( virtual_machine&& )      = delete;

  std::error_code run( host_api& hapi,
                       std::span< const std::byte > bytecode,
                       std::span< const std::byte > id = std::span< const std::byte >() ) noexcept;

private:
  std::unique_ptr< module_cache > _cache;
};

} // namespace respublica::vm
