#pragma once

#include <koinos/controller/call_stack.hpp>
#include <koinos/controller/chronicler.hpp>
#include <koinos/controller/error.hpp>
#include <koinos/controller/resource_meter.hpp>
#include <koinos/controller/session.hpp>
#include <koinos/controller/state.hpp>
#include <koinos/crypto.hpp>
#include <koinos/program.hpp>
#include <koinos/state_db.hpp>
#include <koinos/vm_manager.hpp>

#include <map>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace koinos::controller {

// The need for two maps will be solved when c++-26 adds span literals.
using program_registry_map = std::map< protocol::account, std::unique_ptr< program::program > >;

using program_registry_span_map =
  std::map< std::span< const std::byte >, program_registry_map::const_iterator, decltype( []( std::span< const std::byte > lhs, std::span< const std::byte > rhs )
{
  return std::ranges::lexicographical_compare( lhs, rhs );
} ) >;

enum class intent : std::uint8_t
{
  read_only,
  block_application,
  transaction_application,
  block_proposal
};

class execution_context final: public program::program_interface

{
public:
  execution_context()                           = delete;
  execution_context( const execution_context& ) = delete;
  execution_context( execution_context&& )      = delete;
  execution_context( const std::shared_ptr< vm_manager::vm_backend >&, intent i = intent::read_only );

  ~execution_context() final = default;

  execution_context& operator=( const execution_context& ) = delete;
  execution_context& operator=( execution_context&& )      = delete;

  void set_state_node( const state_db::state_node_ptr& );
  void clear_state_node();

  class resource_meter& resource_meter();
  class chronicler& chronicler();

  result< protocol::block_receipt > apply( const protocol::block& );
  result< protocol::transaction_receipt > apply( const protocol::transaction& );

  std::span< const std::string > program_arguments() final;

  void write( program::file_descriptor fd, std::span< const std::byte > bytes ) final;
  std::error_code read( program::file_descriptor fd, std::span< std::byte > buffer ) final;

  std::span< const std::byte > get_object( std::uint32_t id, std::span< const std::byte > key ) final;

  std::pair< std::span< const std::byte >, std::span< const std::byte > >
  get_next_object( std::uint32_t id, std::span< const std::byte > key ) final;

  std::pair< std::span< const std::byte >, std::span< const std::byte > >
  get_prev_object( std::uint32_t id, std::span< const std::byte > key ) final;

  std::error_code
  put_object( std::uint32_t id, std::span< const std::byte > key, std::span< const std::byte > value ) final;

  std::error_code remove_object( std::uint32_t id, std::span< const std::byte > key ) final;

  void log( std::span< const std::byte > message ) final;

  std::error_code event( std::span< const std::byte > name,
                         std::span< const std::byte > data,
                         const std::vector< std::span< const std::byte > >& impacted ) final;

  result< bool > check_authority( protocol::account_view account ) final;

  std::span< const std::byte > get_caller() final;

  result< protocol::program_output > call_program( protocol::account_view account,
                                                   std::span< const std::byte > stdin,
                                                   std::span< const std::string > arguments = {} ) final;

  std::uint64_t account_resources( protocol::account_view ) const;
  std::uint64_t account_nonce( protocol::account_view ) const;

  const crypto::digest& network_id() const noexcept;
  state::head head() const;
  const state::resource_limits& resource_limits() const;

private:
  std::error_code apply( const protocol::upload_program& );
  std::error_code apply( const protocol::call_program& );
  std::error_code consume_account_resources( protocol::account_view account, std::uint64_t resources );
  std::error_code set_account_nonce( protocol::account_view account, std::uint64_t nonce );

  state_db::object_space create_object_space( std::uint32_t id );

  std::shared_ptr< session > make_session( std::uint64_t );

  std::shared_ptr< vm_manager::vm_backend > _vm_backend;
  state_db::state_node_ptr _state_node;
  call_stack _stack;

  const protocol::block* _block             = nullptr;
  const protocol::transaction* _transaction = nullptr;
  const protocol::operation* _operation     = nullptr;

  class resource_meter _resource_meter;
  class chronicler _chronicler;
  intent _intent;

  std::vector< protocol::account_view > _verified_signatures;

  static const program_registry_map program_registry;
  static const program_registry_span_map program_span_registry;
};

} // namespace koinos::controller
