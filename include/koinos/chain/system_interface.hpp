#pragma once
#include <array>
#include <expected>
#include <span>

using account_t = std::array< std::byte, 25 >;
using bytes_v = std::vector< std::byte >;
using bytes_s = std::span< const std::byte >;

struct system_interface {

  virtual ~system_interface() = default;

  virtual std::expected< std::vector< bytes_v >&, error_code > arguments() = 0;
  virtual error_code write_output( bytes_s bytes ) = 0;

  virtual std::expected< bytes_s, error_code > get_object( uint32_t id, bytes_s key ) = 0;
  virtual std::expected< bytes_s, error_code > get_next_object( uint32_t id, bytes_s key ) = 0;
  virtual std::expected< bytes_s, error_code > get_prev_object( uint32_t id, bytes_s key ) = 0;
  virtual error_code put_object( uint32_t id, bytes_s key, bytes_s value ) = 0;
  virtual error_code remove_object( uint32_t id, bytes_s key ) = 0;

  virtual error_code log( bytes_s message ) = 0;
  virtual error_code event( bytes_s name, bytes_s data, std::vector< account_t >& impacted ) = 0;

  virtual std::expected< bool,error_code > check_authority( bytes_s account ) = 0;

  virtual std::expected< account_t, error_code > get_caller() = 0;

  virtual std::expected< bytes_v, error_code > call_program( bytes_s address, uint32_t entry_point, std::span< bytes_s > args ) = 0;
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

