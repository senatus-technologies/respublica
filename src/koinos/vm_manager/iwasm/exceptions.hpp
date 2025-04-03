#pragma once

#include <koinos/exception.hpp>
#include <koinos/vm_manager/exceptions.hpp>

namespace koinos::vm_manager::iwasm {

KOINOS_DECLARE_DERIVED_EXCEPTION( iwasm_vm_exception, vm_backend_exception );

// Module loading exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( module_parse_exception, iwasm_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( module_instantiate_exception, iwasm_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( module_clone_exception, iwasm_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( create_context_exception, iwasm_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( module_start_exception, iwasm_vm_exception );

// Runtime exceptions
KOINOS_DECLARE_DERIVED_EXCEPTION( wasm_trap_exception, iwasm_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( wasm_memory_exception, iwasm_vm_exception );

// These exceptions should never happen, if they do it's a programming bug
KOINOS_DECLARE_DERIVED_EXCEPTION( iwasm_returned_null_exception, iwasm_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( null_argument_exception, iwasm_vm_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( runner_state_exception, iwasm_vm_exception );

} // namespace koinos::vm_manager::iwasm
