#pragma once

#include <respublica/controller/error.hpp>
#include <respublica/controller/state.hpp>
#include <respublica/protocol.hpp>
#include <respublica/state_db.hpp>
#include <respublica/vm.hpp>

#include <chrono>
#include <filesystem>
#include <memory>

namespace respublica::controller {

class controller
{
public:
  controller( std::uint64_t read_compute_bandwith_limit = 0 );
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
           std::uint64_t index_to                    = 0,
           std::chrono::system_clock::time_point now = std::chrono::system_clock::now() );

  result< protocol::transaction_receipt > process( const protocol::transaction& transaction, bool broadcast = true );

  const crypto::digest& network_id() const noexcept;

  state::head head() const;

  result< protocol::program_output > read_program( const protocol::account& account,
                                                   const protocol::program_input& input = {} ) const;

  std::uint64_t account_nonce( const protocol::account& account ) const;
  std::uint64_t account_resources( const protocol::account& account ) const;

  state::resource_limits resource_limits() const;

private:
  state_db::database _db;
  std::shared_ptr< vm::virtual_machine > _vm;
  std::uint64_t _read_compute_bandwidth_limit;
};

} // namespace respublica::controller
