#pragma once

#include <koinos/error/error.hpp>
#include <koinos/vm_manager/host_api.hpp>

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace koinos::vm_manager {

using koinos::error::error;

/**
 * Abstract class for WebAssembly virtual machines.
 *
 * To add a new WebAssembly VM, you need to implement this class
 * and return it in get_vm_backends().
 */
class vm_backend
{
   public:
      vm_backend() = default;
      vm_backend( const vm_backend& ) = delete;
      vm_backend( vm_backend&& ) = delete;
      virtual ~vm_backend() = default;

      vm_backend& operator =( const vm_backend& ) = delete;
      vm_backend& operator =( vm_backend&& ) = delete;

      virtual std::string backend_name() = 0;

      /**
       * Initialize the backend. Should only be called once if there are no errors.
       */
      virtual void initialize() = 0;

      /**
       * Run some bytecode.
       */
      virtual error run( abstract_host_api& hapi, std::span< const std::byte > bytecode, std::span< const std::byte > id = std::span< const std::byte >() ) = 0;
};

/**
 * Get a list of available VM backends.
 */
std::vector< std::shared_ptr< vm_backend > > get_vm_backends();

std::string get_default_vm_backend_name();

/**
 * Get a shared_ptr to the named VM backend.
 */
std::shared_ptr< vm_backend > get_vm_backend( const std::string& name = get_default_vm_backend_name() );

} // koinos::vm_manager
