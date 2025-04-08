#pragma once
#include <array>
#include <expected>
#include <span>

using account_t = std::array< std::byte, 25 >;
using bytes_v = std::vector< std::byte >;
using bytes_s = std::span< const std::byte >;

struct system_interface {

  virtual ~system_interface() = default;

  virtual std::expected< std::vector< bytes_v >&, koinos_error > arguments() = 0;
  virtual koinos_error write_output( bytes_s bytes ) = 0;

  virtual std::expected< bytes_s, koinos_error > get_object( uint32_t id, bytes_s key ) = 0;
  virtual std::expected< bytes_s, koinos_error > get_next_object( uint32_t id, bytes_s key ) = 0;
  virtual std::expected< bytes_s, koinos_error > get_prev_object( uint32_t id, bytes_s key ) = 0;
  virtual koinos_error put_object( uint32_t id, bytes_s key, bytes_s value ) = 0;
  virtual koinos_error remove_object( uint32_t id, bytes_s key ) = 0;

  virtual koinos_error log( bytes_s message ) = 0;
  virtual koinos_error event( bytes_s name, bytes_s data, std::vector< account_t >& impacted ) = 0;

  virtual std::expected< bool,koinos_error > check_authority( bytes_s account ) = 0;

  virtual std::expected< account_t, koinos_error > get_caller() = 0;

  virtual std::expected< bytes_v, koinos_error > call_program( bytes_s address, uint32_t entry_point, std::span< bytes_s > args ) = 0;
};

// TODO:

// get_transaction_field
// get_block_field ?
// head info

// Potentially implemented as linked module(s)
//std::expected< bytes_v, koinos_error > hash( uint64_t code, bytes_s data );

//std::expected< bytes_v, koinos_error > recover_public_key( uint64_t dsa, bytes_s signature, bytes_s digest, bool compressed );

//std::expected< koinos_error, bool > verify_merkle_root();

//std::expected< koinos_error, bool > verify_signature();

