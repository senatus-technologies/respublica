#pragma once
#include <respublica/protocol/account.hpp>
#include <respublica/state_db/backends/backend.hpp>
#include <respublica/state_db/error.hpp>
#include <respublica/state_db/types.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <vector>

namespace std {
template<>
struct hash< respublica::state_db::state_node_id >
{
  std::size_t operator()( const respublica::state_db::state_node_id& arr ) const
  {
    std::size_t seed = 0;
    for( const auto& value: arr )
    {
      seed ^= std::hash< std::byte >()( value );
    }
    return seed;
  }
};
} // namespace std

namespace respublica::state_db {

class state_delta final: public std::enable_shared_from_this< state_delta >
{
private:
  std::vector< std::shared_ptr< state_delta > > _parents;

  std::shared_ptr< backends::abstract_backend > _backend;
  std::set< std::vector< std::byte > > _removed_objects;
  mutable std::set< std::vector< std::byte > > _read_keys;

  mutable std::optional< digest > _merkle_root;

  bool _complete = false;

  std::map< protocol::account, approval_weight_t > _approvals;
  approval_weight_t _approval_threshold = 0;
  bool _finalized                       = false;

public:
  state_delta() noexcept;
  state_delta( const std::optional< std::filesystem::path >& p ) noexcept;

  state_delta& operator=( const state_delta& ) = delete;
  state_delta& operator=( state_delta&& )      = delete;
  state_delta( const state_delta& )            = delete;
  state_delta( state_delta&& )                 = delete;

  ~state_delta() = default;

  template< std::ranges::range ValueType >
  std::int64_t put( std::vector< std::byte >&& key, const ValueType& value );
  std::int64_t remove( std::vector< std::byte >&& key );
  std::optional< std::span< const std::byte > > get( const std::vector< std::byte >& key ) const;

  void squash();
  void commit();
  void clear();

  bool removed( const std::vector< std::byte >& key ) const;
  bool root() const;

  std::uint64_t revision() const;
  void set_revision( std::uint64_t revision );

  bool complete() const;
  void mark_complete();

  bool finalized() const;
  approval_weight_t total_approval() const;

  const digest& merkle_root() const;

  const state_node_id& id() const;
  const std::vector< std::shared_ptr< state_delta > >& parents() const;

  static result< std::shared_ptr< state_delta > > create_delta( const state_node_id& id,
                                                                std::vector< std::shared_ptr< state_delta > >&& parents,
                                                                const protocol::account& creator,
                                                                approval_weight_t creator_weight,
                                                                approval_weight_t threshold );

  std::shared_ptr< state_delta > make_child( const state_node_id& id                    = null_id,
                                             std::optional< protocol::account > creator = {},
                                             approval_weight_t creator_weight           = 0,
                                             approval_weight_t threshold                = 0 );

  bool has_conflict( const state_delta& other ) const;

private:
  void commit_helper();

  void propagate_approval_to_ancestors( const protocol::account& approver, approval_weight_t weight );
  void finalize_grandparents_if_threshold_met();
};

template< std::ranges::range ValueType >
std::int64_t state_delta::put( std::vector< std::byte >&& key, const ValueType& value )
{
  if( complete() )
    throw std::runtime_error( "cannot modify a complete state delta" );

  std::int64_t size = std::ssize( key ) + std::ssize( value );
  if( auto current_value = get( key ); current_value )
    size -= std::ssize( key ) + std::ssize( *current_value );

  _backend->put( std::move( key ), std::vector< std::byte >( value.begin(), value.end() ) );

  return size;
}

} // namespace respublica::state_db
