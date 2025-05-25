#pragma once

#include <cstddef>
#include <vector>

const std::vector< std::byte >& get_authorize_wasm();
const std::vector< std::byte >& get_benchmark_wasm();
const std::vector< std::byte >& get_call_wasm();
const std::vector< std::byte >& get_contract_return_wasm();
const std::vector< std::byte >& get_db_write_wasm();
const std::vector< std::byte >& get_echo_wasm();
const std::vector< std::byte >& get_empty_contract_wasm();
const std::vector< std::byte >& get_exit_wasm();
const std::vector< std::byte >& get_forever_wasm();
const std::vector< std::byte >& get_get_arguments_override_wasm();
const std::vector< std::byte >& get_hello_wasm();
const std::vector< std::byte >& get_koin_wasm();
const std::vector< std::byte >& get_null_bytes_written_wasm();
const std::vector< std::byte >& get_syscall_override_wasm();
const std::vector< std::byte >& get_syscall_rpc_wasm();

const std::vector< std::byte >& get_call_contract_wasm();
const std::vector< std::byte >& get_call_system_call_wasm();
const std::vector< std::byte >& get_call_system_call2_wasm();
const std::vector< std::byte >& get_stack_assertion_wasm();
const std::vector< std::byte >& get_system_from_system_wasm();
const std::vector< std::byte >& get_system_from_user_wasm();
const std::vector< std::byte >& get_user_from_system_wasm();
const std::vector< std::byte >& get_user_from_user_wasm();
