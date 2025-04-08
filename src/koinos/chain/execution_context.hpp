#pragma once

#include <google/protobuf/descriptor.h>

#include <koinos/chain/chronicler.hpp>
#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/resource_meter.hpp>
#include <koinos/chain/session.hpp>
#include <koinos/chain/system_interface.hpp>
#include <koinos/crypto/elliptic.hpp>
#include <koinos/state_db/state_db.hpp>
#include <koinos/vm_manager/vm_backend.hpp>

#include <koinos/chain/chain.pb.h>
#include <koinos/chain/system_call_ids.pb.h>
#include <koinos/chain/value.pb.h>
#include <koinos/protocol/protocol.pb.h>

#include <memory>
#include <span>
#include <string>
#include <utility>
#include <variant>

namespace koinos::chain {

namespace constants {
const std::string system = std::string{};
} // namespace constants

using koinos::state_db::abstract_state_node;
using koinos::state_db::anonymous_state_node_ptr;
using koinos::state_db::state_node_ptr;

using abstract_state_node_ptr = std::shared_ptr< abstract_state_node >;
using receipt                 = std::variant< std::monostate, protocol::block_receipt, protocol::transaction_receipt >;

struct stack_frame
{
  bytes_s contract_id;
  std::vector< bytes_v > arguments;
  uint32_t entry_point = 0;
  bytes_v output;
};

enum class intent : uint64_t
{
  read_only,
  block_application,
  transaction_application,
  block_proposal
};

class execution_context : public system_interface
{
public:
  execution_context() = delete;
  execution_context( std::shared_ptr< vm_manager::vm_backend >, chain::intent i = chain::intent::read_only );

  virtual ~execution_context() = default;

  static constexpr std::size_t stack_limit = 256;

  void set_state_node( abstract_state_node_ptr );
  void clear_state_node();

  koinos_error apply_block( const protocol::block& );
  koinos_error apply_transaction( const protocol::transaction& );

  chain::resource_meter& resource_meter();
  chain::chronicler& chronicler();
  chain::receipt& receipt();

  std::expected< std::vector< std::vector::< std::byte > >&, koinos_error > arguments() override;
  koinos_error write_output( bytes_s bytes ) override;

  std::expected< bytes_s, koinos_error > get_object( uint32_t id, bytes_s key ) override;
  std::expected< bytes_s, koinos_error > get_next_object( uint32_t id, bytes_s key ) override;
  std::expected< bytes_s, koinos_error > get_prev_object( uint32_t id, bytes_s key ) override;
  koinos_error put_object( uint32_t id, bytes_s key, bytes_s value ) override;
  koinos_error remove_object( uint32_t id, bytes_s key ) override;

  koinos_error log( bytes_s message ) override;
  koinos_error event( bytes_s name, bytes_s data, std::vector< account_t >& impacted ) override;

  std::expected< bool, koinos_error > check_authority( bytes_s account ) override;

  std::expected< account_t&, koinos_error > get_caller() override;

  std::expected< bytes_v, koinos_error > call_program( bytes_s address, uint32_t entry_point, std::span< bytes_s > args ) override;
  std::expected< bytes_v, koinos_error > call_program_priviledged( bytes_s address, uint32_t entry_point, std::span< bytes_s > args )

private:
  koinos_error apply_operation( const protocol::operation& );
  koinos_error consume_account_rc( bytes_s account, uint64_t rc );
  koinos_error verify_account_nonce( bytes_s account, bytes_s nonce );

  std::expected< object_space, koinos_error > create_object_space( uint32_t id );

  koinos_error push_frame( stack_frame&& f );
  void stack_frame pop_frame();

  std::shared_ptr< session > make_session( uint64_t );

  std::shared_ptr< vm_manager::vm_backend > _vm_backend;
  std::vector< stack_frame > _stack;

  abstract_state_node_ptr _state_node;

  const protocol::block* _block           = nullptr;
  const protocol::transaction* _trx       = nullptr;
  const protocol::operation* _op          = nullptr;

  resource_meter _resource_meter;
  chronicler _chronicler;
  receipt _receipt;
  intent _intent;
};

namespace detail {

} // namespace detail

template< typename Lambda >
auto with_stack_frame( execution_context& ctx, stack_frame&& f, Lambda&& l )
{
  detail::frame_guard r( ctx, std::move( f ) );
  return l();
}

template< typename Lambda >
  requires std::is_same_v< std::invoke_result_t< Lambda >, void >
auto with_stack_frame( execution_context& ctx, stack_frame&& f, Lambda&& l )
{
  detail::frame_guard r( ctx, std::move( f ) );
  l();
}

} // namespace koinos::chain
