#pragma once

#include <koinos/chain/types.hpp>
#include <koinos/error/error.hpp>
#include <koinos/protocol/protocol.hpp>

#include <expected>
#include <span>
#include <vector>

namespace koinos::chain {

struct system_interface
{
  virtual std::expected< uint32_t, error > contract_entry_point()                = 0;
  virtual const std::vector< std::span< const std::byte > >& program_arguments() = 0;
  virtual error write_output( std::span< const std::byte > bytes )               = 0;

  virtual std::expected< std::span< const std::byte >, error > get_object( uint32_t id,
                                                                           std::span< const std::byte > key ) = 0;
  virtual std::expected< std::pair< std::span< const std::byte >, std::span< const std::byte > >, error >
  get_next_object( uint32_t id, std::span< const std::byte > key ) = 0;
  virtual std::expected< std::pair< std::span< const std::byte >, std::span< const std::byte > >, error >
  get_prev_object( uint32_t id, std::span< const std::byte > key )                                              = 0;
  virtual error put_object( uint32_t id, std::span< const std::byte > key, std::span< const std::byte > value ) = 0;
  virtual error remove_object( uint32_t id, std::span< const std::byte > key )                                  = 0;

  virtual std::expected< void, error > log( std::span< const std::byte > message )   = 0;
  virtual error event( std::span< const std::byte > name,
                       std::span< const std::byte > data,
                       const std::vector< std::span< const std::byte > >& impacted ) = 0;

  virtual std::expected< bool, error > check_authority( const protocol::account& account ) = 0;

  virtual std::expected< std::span< const std::byte >, error > get_caller() = 0;

  virtual std::expected< std::vector< std::byte >, error >
  call_program( const protocol::account& account,
                uint32_t entry_point,
                const std::vector< std::span< const std::byte > >& args ) = 0;
};

// TODO:

// get_transaction_field
// get_block_field ?
// head info

// Potentially implemented as linked module(s)
// std::expected< std::vector< std::byte >, error_code > hash( uint64_t code, std::span< const std::byte > data );

// std::expected< std::vector< std::byte >, error_code > recover_public_key( uint64_t dsa, std::span< const std::byte >
// signature, bytes_s digest, bool compressed );

// std::expected< error_code, bool > verify_merkle_root();

// std::expected< error_code, bool > verify_signature();

} // namespace koinos::chain
