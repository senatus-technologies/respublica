#pragma once

#include <koinos/program/error.hpp>
#include <koinos/protocol.hpp>

#include <memory>
#include <span>

namespace koinos::program {

enum class file_descriptor : int // NOLINT(performance-enum-size)
{
  stdin,
  stdout,
  stderr
};

struct system_interface
{
  system_interface()                          = default;
  system_interface( const system_interface& ) = delete;
  system_interface( system_interface&& )      = delete;
  virtual ~system_interface()                 = default;

  system_interface& operator=( const system_interface& ) = delete;
  system_interface& operator=( system_interface&& )      = delete;

  virtual std::span< const std::string > arguments()                                       = 0;
  virtual std::error_code write( file_descriptor fd, std::span< const std::byte > buffer ) = 0;
  virtual std::error_code read( file_descriptor fd, std::span< std::byte > buffer )        = 0;

  virtual std::span< const std::byte > get_object( std::uint32_t id, std::span< const std::byte > key ) = 0;
  virtual std::pair< std::span< const std::byte >, std::span< const std::byte > >
  get_next_object( std::uint32_t id, std::span< const std::byte > key ) = 0;

  virtual std::pair< std::span< const std::byte >, std::span< const std::byte > >
  get_prev_object( std::uint32_t id, std::span< const std::byte > key ) = 0;

  virtual std::error_code
  put_object( std::uint32_t id, std::span< const std::byte > key, std::span< const std::byte > value ) = 0;

  virtual std::error_code remove_object( std::uint32_t id, std::span< const std::byte > key ) = 0;

  virtual result< bool > check_authority( protocol::account_view account ) = 0;

  virtual std::span< const std::byte > get_caller() = 0;

  virtual result< protocol::program_output* > call_program( protocol::account_view account,
                                                            std::span< const std::byte > stdin,
                                                            std::span< const std::string > arguments = {} ) = 0;
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

} // namespace koinos::program
