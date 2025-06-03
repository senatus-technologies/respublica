#pragma once

#include <koinos/chain/state.hpp>
#include <koinos/protocol/protocol.hpp>
#include <koinos/state_db/state_db.hpp>
#include <koinos/vm_manager/vm_backend.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <shared_mutex>

namespace koinos::chain {

class controller
{
public:
  controller( uint64_t read_compute_bandwith_limit = 0 );
  ~controller();

  void
  open( const std::filesystem::path& p, const state::genesis_data& data, state_db::fork_resolution_algorithm algo, bool reset );
  void close();

  std::expected< protocol::block_receipt, error::error >
  process( const protocol::block& block,
           uint64_t index_to                         = 0,
           std::chrono::system_clock::time_point now = std::chrono::system_clock::now() );

  std::expected< protocol::transaction_receipt, error::error > process( const protocol::transaction& transaction,
                                                                        bool broadcast = true );

  protocol::digest network_id() const;

  state::head head() const;

  std::expected< protocol::program_output, error::error >
  read_program( const protocol::account& account,
                uint64_t entry_point,
                const std::vector< std::vector< std::byte > >& arguments = {} ) const;

  uint64_t account_nonce( const protocol::account& account ) const;
  uint64_t account_rc( const protocol::account& account ) const;

  state::resource_limits resource_limits() const;

private:
  state_db::database _db;
  std::shared_ptr< vm_manager::vm_backend > _vm_backend;
  uint64_t _read_compute_bandwidth_limit;
  mutable std::shared_mutex _cached_head_block_mutex;
  std::shared_ptr< const protocol::block > _cached_head_block;

  error::error validate( const protocol::block& b );
  error::error validate( const protocol::transaction& t );
};

} // namespace koinos::chain
