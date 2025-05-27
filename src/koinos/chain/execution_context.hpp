#pragma once

#include <koinos/chain/call_stack.hpp>
#include <koinos/chain/chronicler.hpp>
#include <koinos/chain/coin.hpp>
#include <koinos/chain/program.hpp>
#include <koinos/chain/resource_meter.hpp>
#include <koinos/chain/session.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/system_interface.hpp>
#include <koinos/crypto/crypto.hpp>
#include <koinos/state_db/state_db.hpp>
#include <koinos/vm_manager/vm_backend.hpp>

#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace koinos::chain {

using program_registry_map = std::unordered_map< protocol::account, std::unique_ptr< program > >;

namespace constants {
const std::string system = std::string{};
} // namespace constants

using koinos::state_db::abstract_state_node_ptr;

enum class intent : uint64_t
{
  read_only,
  block_application,
  transaction_application,
  block_proposal
};

class execution_context: public system_interface

{
public:
  execution_context() = delete;
  execution_context( std::shared_ptr< vm_manager::vm_backend >, chain::intent i = chain::intent::read_only );

  virtual ~execution_context() = default;

  void set_state_node( abstract_state_node_ptr );
  void clear_state_node();

  chain::resource_meter& resource_meter();
  chain::chronicler& chronicler();

  std::expected< protocol::block_receipt, error > apply( const protocol::block& );
  std::expected< protocol::transaction_receipt, error > apply( const protocol::transaction& );

  std::expected< uint32_t, error > contract_entry_point() override;
  std::expected< std::span< const bytes_v >, error > contract_arguments() override;
  error write_output( bytes_s bytes ) override;

  std::expected< bytes_s, error > get_object( uint32_t id, bytes_s key ) override;
  std::expected< std::pair< bytes_s, bytes_v >, error > get_next_object( uint32_t id, bytes_s key ) override;
  std::expected< std::pair< bytes_s, bytes_v >, error > get_prev_object( uint32_t id, bytes_s key ) override;
  error put_object( uint32_t id, bytes_s key, bytes_s value ) override;
  error remove_object( uint32_t id, bytes_s key ) override;

  std::expected< void, error > log( bytes_s message ) override;
  error event( bytes_s name, bytes_s data, const std::vector< bytes_s >& impacted ) override;

  std::expected< bool, error > check_authority( const protocol::account& account ) override;

  std::expected< bytes_s, error > get_caller() override;

  std::expected< bytes_v, error >
  call_program( const protocol::account& account, uint32_t entry_point, const std::vector< bytes_s >& args ) override;

  uint64_t account_rc( const protocol::account& ) const;
  uint64_t account_nonce( const protocol::account& ) const;

  protocol::digest network_id() const;
  state::head head() const;
  state::resource_limits resource_limits() const;
  uint64_t last_irreversible_block() const;
  crypto::multihash state_merkle_root() const;

private:
  error apply( const protocol::upload_program& );
  error apply( const protocol::call_program& );
  error consume_account_rc( const protocol::account& account, uint64_t rc );
  error set_account_nonce( const protocol::account& account, uint64_t nonce );

  std::expected< bytes_v, error >
  call_program_privileged( const protocol::account& account, uint32_t entry_point, const std::vector< bytes_s >& args );

  state_db::object_space create_object_space( uint32_t id );

  std::shared_ptr< session > make_session( uint64_t );

  std::shared_ptr< vm_manager::vm_backend > _vm_backend;
  abstract_state_node_ptr _state_node;
  call_stack _stack;

  const protocol::block* _block     = nullptr;
  const protocol::transaction* _trx = nullptr;
  const protocol::operation* _op    = nullptr;

  chain::resource_meter _resource_meter;
  chain::chronicler _chronicler;
  intent _intent;

  std::vector< protocol::account > _recovered_signatures;

  const program_registry_map program_registry = []()
  {
    program_registry_map registry;
    registry[ protocol::system_account( "coin" ) ] = std::make_unique< coin >();
    return registry;
  }();
};

} // namespace koinos::chain
