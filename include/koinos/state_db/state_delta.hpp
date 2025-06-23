#pragma once
#include <koinos/state_db/backends/backend.hpp>
#include <koinos/state_db/types.hpp>

#include <filesystem>
#include <memory>
#include <ranges>
#include <set>
#include <vector>

namespace std {
template<>
struct hash< koinos::state_db::state_node_id >
{
  std::size_t operator()( const koinos::state_db::state_node_id& arr ) const
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

namespace koinos::state_db {

class state_delta final: public std::enable_shared_from_this< state_delta >
{
private:
  std::shared_ptr< state_delta > _parent;

  std::shared_ptr< backends::abstract_backend > _backend;
  std::set< std::vector< std::byte > > _removed_objects;

  mutable std::optional< digest > _merkle_root;

  bool _final = false;

public:
  state_delta() noexcept;
  state_delta& operator=( const state_delta& ) = delete;
  state_delta& operator=( state_delta&& )      = delete;
  state_delta( const std::optional< std::filesystem::path >& p ) noexcept;
  state_delta( const state_delta& ) = delete;
  state_delta( state_delta&& )      = delete;
  ~state_delta()                    = default;

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

  bool final() const;
  void finalize();

  const digest& merkle_root() const;

  const state_node_id& id() const;
  const state_node_id& parent_id() const;
  std::shared_ptr< state_delta > parent() const;

  std::shared_ptr< state_delta > make_child( const state_node_id& id = null_id );
  std::shared_ptr< state_delta > clone( const state_node_id& id = null_id ) const;

private:
  void commit_helper();
};

template< std::ranges::range ValueType >
std::int64_t state_delta::put( std::vector< std::byte >&& key, const ValueType& value )
{
  if( final() )
    throw std::runtime_error( "cannot modify a final state delta" );

  std::int64_t size = 0;
  if( !root() )
    if( auto parent_value = _parent->get( key ); parent_value )
      size -= std::ssize( key ) + std::ssize( *parent_value );

  return size + _backend->put( std::move( key ), std::vector< std::byte >( value.begin(), value.end() ) );
}

} // namespace koinos::state_db
