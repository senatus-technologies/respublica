#pragma once

#include <koinos/program/error.hpp>
#include <koinos/protocol.hpp>

#include <span>
#include <vector>

namespace koinos::program {

enum class file_descriptor : int // NOLINT(performance-enum-size)
{
  stdin,
  stdout,
  stderr
};

struct program_interface
{
  program_interface()                           = default;
  program_interface( const program_interface& ) = delete;
  program_interface( program_interface&& )      = delete;
  virtual ~program_interface()                  = default;

  program_interface& operator=( const program_interface& ) = delete;
  program_interface& operator=( program_interface&& )      = delete;

  virtual std::span< const std::string > program_arguments()                        = 0;
  virtual void write( file_descriptor fd, std::span< const std::byte > buffer )     = 0;
  virtual std::error_code read( file_descriptor fd, std::span< std::byte > buffer ) = 0;

  virtual std::span< const std::byte > get_object( std::uint32_t id, std::span< const std::byte > key ) = 0;
  virtual std::pair< std::span< const std::byte >, std::span< const std::byte > >
  get_next_object( std::uint32_t id, std::span< const std::byte > key ) = 0;

  virtual std::pair< std::span< const std::byte >, std::span< const std::byte > >
  get_prev_object( std::uint32_t id, std::span< const std::byte > key ) = 0;

  virtual std::error_code
  put_object( std::uint32_t id, std::span< const std::byte > key, std::span< const std::byte > value ) = 0;

  virtual std::error_code remove_object( std::uint32_t id, std::span< const std::byte > key ) = 0;

  virtual void log( std::span< const std::byte > message )                                     = 0;
  virtual std::error_code event( std::span< const std::byte > name,
                                 std::span< const std::byte > data,
                                 const std::vector< std::span< const std::byte > >& impacted ) = 0;

  virtual result< bool > check_authority( protocol::account_view account ) = 0;

  virtual std::span< const std::byte > get_caller() = 0;

  virtual result< protocol::program_output > call_program( protocol::account_view account,
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
