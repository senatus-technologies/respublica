#pragma once

#include <koinos/vm_manager/iwasm/module_cache.hpp>
#include <koinos/vm_manager/vm_backend.hpp>

#include <koinos/chain/chain.pb.h>

#include <string>

namespace koinos::vm_manager::iwasm {

/**
 * Implementation of vm_backend for iwasm.
 */
class iwasm_vm_backend: public vm_backend
{
public:
  iwasm_vm_backend();
  virtual ~iwasm_vm_backend();

  std::string backend_name() override;
  error_code initialize() override;

  error_code run( abstract_host_api& hapi, const std::string& bytecode, const std::string& id = std::string() ) override;

private:
  module_cache _cache;
};

} // namespace koinos::vm_manager::iwasm
