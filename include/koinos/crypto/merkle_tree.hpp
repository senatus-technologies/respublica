#pragma once

#include <memory>
#include <optional>

#include <koinos/crypto/hash.hpp>

namespace koinos::crypto {

template< class T, bool hashed = false >
class merkle_node
{
public:
  merkle_node():
      _left( nullptr ),
      _right( nullptr )
  {
    _hash = crypto::hash( nullptr );
  }

  merkle_node( const T& value ):
      _left( nullptr ),
      _right( nullptr )
  {
    if constexpr( hashed )
      _hash = value;
    else
      _hash = crypto::hash( value );
    _value = value;
  }

  merkle_node( std::unique_ptr< merkle_node< T, hashed > > l, std::unique_ptr< merkle_node< T, hashed > > r ):
      _left( std::move( l ) ),
      _right( std::move( r ) )
  {
#pragma message( "Can we avoid this copy?" )
    std::vector< std::byte > buffer;
    std::copy( left()->hash().begin(), left()->hash().end(), std::back_inserter( buffer ) );
    std::copy( right()->hash().begin(), right()->hash().end(), std::back_inserter( buffer ) );
    _hash = crypto::hash( reinterpret_cast< const void* >( buffer.data() ), buffer.size() );
  }

  const digest& hash() const
  {
    return _hash;
  }

  const std::unique_ptr< merkle_node< T, hashed > >& left() const
  {
    return _left;
  }

  const std::unique_ptr< merkle_node< T, hashed > >& right() const
  {
    return _right;
  }

  const std::optional< T >& value() const
  {
    return _value;
  }

private:
  std::unique_ptr< merkle_node< T, hashed > > _left, _right;
  digest _hash;
  std::optional< T > _value;
};

template< class T, bool hashed = false >
class merkle_tree
{
  using node_type = merkle_node< T, hashed >;

  merkle_tree( std::unique_ptr< node_type > root ):
      _root( std::move( root ) )
  {}

public:
  static merkle_tree< T, hashed > create( const std::vector< T >& elements )
  {
    if( !elements.size() )
      return merkle_tree< T, hashed >( std::make_unique< node_type >() );

    std::vector< std::unique_ptr< node_type > > nodes;

    for( auto& e: elements )
      nodes.push_back( std::make_unique< node_type >( e ) );

    auto count = nodes.size();

    while( count > 1 )
    {
      std::vector< std::unique_ptr< node_type > > next;

      for( std::size_t index = 0; index < nodes.size(); index++ )
      {
        auto left = std::move( nodes[ index ] );

        if( index + 1 < nodes.size() )
        {
          auto right = std::move( nodes[ ++index ] );
          next.push_back( std::make_unique< node_type >( std::move( left ), std::move( right ) ) );
        }
        else
          next.push_back( std::move( left ) );
      }

      count = next.size();
      nodes = std::move( next );
    }

    return merkle_tree< T, hashed >( std::move( nodes.front() ) );
  }

  const std::unique_ptr< node_type >& root()
  {
    return _root;
  }

private:
  std::unique_ptr< node_type > _root;
};

} // namespace koinos::crypto
