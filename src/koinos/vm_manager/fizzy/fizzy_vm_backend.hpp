#pragma once

#include <koinos/vm_manager/fizzy/module_cache.hpp>
#include <koinos/vm_manager/vm_backend.hpp>

#include <string>

namespace koinos::vm_manager::fizzy {

/**
 * Implementation of vm_backend for Fizzy.
 */
class fizzy_vm_backend: public vm_backend
{
public:
  fizzy_vm_backend()                                     = default;
  fizzy_vm_backend( const fizzy_vm_backend& )            = delete;
  fizzy_vm_backend( fizzy_vm_backend&& )                 = delete;
  fizzy_vm_backend& operator=( const fizzy_vm_backend& ) = delete;
  fizzy_vm_backend& operator=( fizzy_vm_backend&& )      = delete;
  ~fizzy_vm_backend() override                           = default;

  std::string backend_name() final;
  void initialize() final;

  std::error_code run( abstract_host_api& hapi,
                       std::span< const std::byte > bytecode,
                       std::span< const std::byte > id = std::span< std::byte >() ) noexcept final;

private:
  module_cache _cache;
};

} // namespace koinos::vm_manager::fizzy
