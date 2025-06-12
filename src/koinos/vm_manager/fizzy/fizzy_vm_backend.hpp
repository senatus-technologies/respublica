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
  virtual ~fizzy_vm_backend() = default;

  std::string backend_name() override;
  void initialize() override;

  error run( abstract_host_api& hapi, std::span< const std::byte > bytecode, std::span< const std::byte > id = std::span< std::byte >() ) override;

private:
  module_cache _cache;
};

} // namespace koinos::vm_manager::fizzy
