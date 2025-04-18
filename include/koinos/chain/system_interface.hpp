#pragma once

#include <koinos/chain/types.hpp>

#include <koinos/error/error.hpp>

#include <array>
#include <expected>
#include <span>
#include <vector>

namespace koinos::chain {

struct system_interface {
  virtual std::expected< uint32_t, error > contract_entry_point() = 0;
  virtual std::expected< std::span< const bytes_v >, error > contract_arguments() = 0;
  virtual error write_output( bytes_s bytes ) = 0;

  virtual std::expected< bytes_s, error > get_object( uint32_t id, bytes_s key ) = 0;
  virtual std::expected< std::pair< bytes_s, bytes_v >, error > get_next_object( uint32_t id, bytes_s key ) = 0;
  virtual std::expected< std::pair< bytes_s, bytes_v >, error > get_prev_object( uint32_t id, bytes_s key ) = 0;
  virtual error put_object( uint32_t id, bytes_s key, bytes_s value ) = 0;
  virtual error remove_object( uint32_t id, bytes_s key ) = 0;

  virtual std::expected< void, error > log( bytes_s message ) = 0;
  virtual error event( bytes_s name, bytes_s data, const std::vector< bytes_s >& impacted ) = 0;

  virtual std::expected< bool,error > check_authority( bytes_s account ) = 0;

  virtual std::expected< bytes_s, error > get_caller() = 0;

  virtual std::expected< bytes_v, error > call_program( bytes_s address, uint32_t entry_point, const std::vector< bytes_s >& args ) = 0;
};

// TODO:

// get_transaction_field
// get_block_field ?
// head info

// Potentially implemented as linked module(s)
//std::expected< bytes_v, error_code > hash( uint64_t code, bytes_s data );

//std::expected< bytes_v, error_code > recover_public_key( uint64_t dsa, bytes_s signature, bytes_s digest, bool compressed );

//std::expected< error_code, bool > verify_merkle_root();

//std::expected< error_code, bool > verify_signature();

} // namespace koinos::chain
