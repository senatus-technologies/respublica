#pragma once

#include <koinos/controller/error.hpp>
#include <koinos/protocol/protocol.hpp>

#include <expected>
#include <span>
#include <vector>

namespace koinos::controller {

struct system_interface
{
  system_interface()                          = default;
  system_interface( const system_interface& ) = delete;
  system_interface( system_interface&& )      = delete;
  virtual ~system_interface()                 = default;

  system_interface& operator=( const system_interface& ) = delete;
  system_interface& operator=( system_interface&& )      = delete;

  virtual std::span< const std::span< const std::byte > > program_arguments() = 0;
  virtual std::error_code write_output( std::span< const std::byte > bytes )  = 0;

  virtual result< std::span< const std::byte > > get_object( std::uint32_t id, std::span< const std::byte > key ) = 0;
  virtual result< std::pair< std::span< const std::byte >, std::span< const std::byte > > >
  get_next_object( std::uint32_t id, std::span< const std::byte > key ) = 0;
  virtual result< std::pair< std::span< const std::byte >, std::span< const std::byte > > >
  get_prev_object( std::uint32_t id, std::span< const std::byte > key ) = 0;
  virtual std::error_code
  put_object( std::uint32_t id, std::span< const std::byte > key, std::span< const std::byte > value ) = 0;
  virtual std::error_code remove_object( std::uint32_t id, std::span< const std::byte > key )          = 0;

  virtual std::error_code log( std::span< const std::byte > message )                          = 0;
  virtual std::error_code event( std::span< const std::byte > name,
                                 std::span< const std::byte > data,
                                 const std::vector< std::span< const std::byte > >& impacted ) = 0;

  virtual result< bool > check_authority( std::span< const std::byte > account ) = 0;

  virtual result< std::span< const std::byte > > get_caller() = 0;

  virtual result< std::vector< std::byte > >
  call_program( std::span< const std::byte > account, const std::span< const std::span< const std::byte > > args ) = 0;
};

// TODO:

// get_transaction_field
// get_block_field ?
// head info

// Potentially implemented as linked module(s)
// std::expected< std::vector< std::byte >, error_code > hash( std::uint64_t code, std::span< const std::byte > data );

// std::expected< std::vector< std::byte >, error_code > recover_public_key( std::uint64_t dsa, std::span< const
// std::byte > signature, bytes_s digest, bool compressed );

// std::expected< error_code, bool > verify_merkle_root();

// std::expected< error_code, bool > verify_signature();

} // namespace koinos::controller
