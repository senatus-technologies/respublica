#pragma once

#include <koinos/chain/error.hpp>
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
  controller( const controller& ) = delete;
  controller( controller&& )      = delete;
  ~controller();

  controller& operator=( const controller& ) = delete;
  controller& operator=( controller&& )      = delete;

  void open( const std::filesystem::path& p,
             const state::genesis_data& data,
             state_db::fork_resolution_algorithm algo,
             bool reset );
  void close();

  result< protocol::block_receipt >
  process( const protocol::block& block,
           uint64_t index_to                         = 0,
           std::chrono::system_clock::time_point now = std::chrono::system_clock::now() );

  result< protocol::transaction_receipt > process( const protocol::transaction& transaction, bool broadcast = true );

  const crypto::digest& network_id() const noexcept;

  state::head head() const;

  result< protocol::program_output >
  read_program( const protocol::account& account,
                uint64_t entry_point,
                const std::vector< std::vector< std::byte > >& arguments = {} ) const;

  uint64_t account_nonce( const protocol::account& account ) const;
  uint64_t account_resources( const protocol::account& account ) const;

  state::resource_limits resource_limits() const;

private:
  state_db::database _db;
  std::shared_ptr< vm_manager::vm_backend > _vm_backend;
  uint64_t _read_compute_bandwidth_limit;
  mutable std::shared_mutex _cached_head_block_mutex;
  std::shared_ptr< const protocol::block > _cached_head_block;
};

} // namespace koinos::chain
