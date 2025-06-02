#pragma once

#include <memory>
#include <ranges>
#include <span>
#include <type_traits>

#include <koinos/crypto/hash.hpp>

namespace koinos::crypto {

template< typename ValueType, bool hashed = false >
class merkle_node final
{
public:
  merkle_node() noexcept:
      _left( nullptr ),
      _right( nullptr )
  {}

  merkle_node( const ValueType& value ) noexcept
    requires( std::is_same_v< ValueType, std::span< const std::byte > > )
      :
      _left( nullptr ),
      _right( nullptr )
  {
    _hash = crypto::hash( reinterpret_cast< const void* >( value.data() ), value.size() );
  }

  merkle_node( const ValueType& value ) noexcept
    requires( std::is_same_v< std::bool_constant< hashed >, std::true_type >
              && std::is_same_v< ValueType, crypto::digest > )
      :
      _left( nullptr ),
      _right( nullptr )
  {
    _hash = value;
  }

  merkle_node( const ValueType& value ) noexcept:
      _left( nullptr ),
      _right( nullptr )
  {
    _hash = crypto::hash( value );
  }

  merkle_node( std::unique_ptr< merkle_node< ValueType, hashed > > l,
               std::unique_ptr< merkle_node< ValueType, hashed > > r ) noexcept:
      _left( std::move( l ) ),
      _right( std::move( r ) )
  {
    hasher_reset();
    hasher_update( left()->hash() );
    hasher_update( right()->hash() );
    _hash = hasher_finalize();
  }

  merkle_node( const merkle_node& other ) noexcept     = default;
  merkle_node( merkle_node&& other ) noexcept          = default;
  merkle_node& operator=( merkle_node& rhs ) noexcept  = default;
  merkle_node& operator=( merkle_node&& rhs ) noexcept = default;
  ~merkle_node() noexcept                              = default;

  const digest& hash() const
  {
    return _hash;
  }

  const std::unique_ptr< merkle_node< ValueType, hashed > >& left() const noexcept
  {
    return _left;
  }

  const std::unique_ptr< merkle_node< ValueType, hashed > >& right() const noexcept
  {
    return _right;
  }

private:
  std::unique_ptr< merkle_node< ValueType, hashed > > _left, _right;
  digest _hash{};
};

template< typename ValueType, bool hashed = false >
class merkle_tree final
{
  using node_type = merkle_node< ValueType, hashed >;

  merkle_tree( std::unique_ptr< node_type > root ) noexcept:
      _root( std::move( root ) )
  {}

public:
  merkle_tree( const merkle_tree& other ) noexcept          = default;
  merkle_tree( merkle_tree&& other ) noexcept               = default;
  merkle_tree& operator=( const merkle_tree& rhs ) noexcept = default;
  merkle_tree& operator=( merkle_tree&& rhs ) noexcept      = default;
  ~merkle_tree() noexcept                                   = default;

  template< typename Range >
    requires std::ranges::range< Range >
  static merkle_tree< ValueType, hashed > create( Range&& values ) noexcept
  {
    if( !values.size() )
      return merkle_tree< ValueType, hashed >( std::make_unique< node_type >() );

    std::vector< std::unique_ptr< node_type > > nodes;
    nodes.reserve( values.size() );

    for( const auto& value: values )
      nodes.emplace_back( std::make_unique< node_type >( value ) );

    auto count = nodes.size();

    while( count > 1 )
    {
      std::size_t next_index = 0;

      for( std::size_t index = 0; index < count; ++index )
      {
        auto left = std::move( nodes[ index ] );

        if( index + 1 < count )
        {
          auto right            = std::move( nodes[ ++index ] );
          nodes[ next_index++ ] = std::make_unique< node_type >( std::move( left ), std::move( right ) );
        }
        else
        {
          nodes[ next_index++ ] = std::move( left );
        }
      }

      count = next_index;
    }

    return merkle_tree< ValueType, hashed >( std::move( nodes.front() ) );
  }

  const std::unique_ptr< node_type >& root() const noexcept
  {
    return _root;
  }

private:
  std::unique_ptr< node_type > _root;
};

template< bool hashed = false, typename Range >
  requires std::ranges::range< Range >
static digest merkle_root( Range&& values ) noexcept
{
  if( !values.size() )
    return digest{};

  std::vector< digest > nodes;
  nodes.reserve( values.size() );

  for( const auto& value: values )
  {
    if constexpr( hashed && std::is_same_v< std::ranges::range_value_t< Range >, crypto::digest > )
      nodes.emplace_back( value );
    else if constexpr( std::is_same_v< std::ranges::range_value_t< Range >, std::span< const std::byte > > )
      nodes.emplace_back( hash( reinterpret_cast< const void* >( value.data() ), value.size() ) );
    else
      nodes.emplace_back( hash( value ) );
  }

  auto count = nodes.size();

  while( count > 1 )
  {
    std::size_t next_index = 0;

    for( std::size_t index = 0; index < count; ++index )
    {
      if( index + 1 < count )
      {
        hasher_reset();
        hasher_update( nodes[ index ] );
        hasher_update( nodes[ ++index ] );
        nodes[ next_index++ ] = hasher_finalize();
      }
      else
      {
        nodes[ next_index++ ] = std::move( nodes[ index ] );
      }
    }

    count = next_index;
  }

  return nodes.front();
}

} // namespace koinos::crypto
