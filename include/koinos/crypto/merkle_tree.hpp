#pragma once

#include <memory>
#include <optional>

#include <koinos/crypto/multihash.hpp>

namespace koinos::crypto {

template< class T >
class merkle_node
{
public:
  merkle_node( multicodec code ):
      _left( nullptr ),
      _right( nullptr )
  {
    if( auto hash = multihash::empty( code ); hash )
      _hash = *hash;
    else
      throw std::runtime_error( std::string( hash.error().message() ) );
  }

  merkle_node( multicodec code, const T& value ):
      _left( nullptr ),
      _right( nullptr )
  {
    if constexpr( std::is_same_v< std::decay_t< T >, multihash > )
    {
      _hash = value;
    }
    else
    {
      if( auto hash = crypto::hash( code, value ); hash )
        _hash = std::move( *hash );
      else
        throw std::runtime_error( std::string( hash.error().message() ) );

      _value = value;
    }
  }

  merkle_node( multicodec code, std::unique_ptr< merkle_node< T > > l, std::unique_ptr< merkle_node< T > > r ):
      _left( std::move( l ) ),
      _right( std::move( r ) )
  {
    std::vector< std::byte > buffer;
    std::copy( left()->hash().digest().begin(), left()->hash().digest().end(), std::back_inserter( buffer ) );
    std::copy( right()->hash().digest().begin(), right()->hash().digest().end(), std::back_inserter( buffer ) );

    if( auto hash = crypto::hash( code, buffer ); hash )
        _hash = std::move( *hash );
      else
        throw std::runtime_error( std::string( hash.error().message() ) );
  }

  const multihash& hash() const
  {
    return _hash;
  }

  const std::unique_ptr< merkle_node< T > >& left() const
  {
    return _left;
  }

  const std::unique_ptr< merkle_node< T > >& right() const
  {
    return _right;
  }

  const std::optional< T >& value() const
  {
    return _value;
  }

private:
  std::unique_ptr< merkle_node< T > > _left, _right;
  multihash _hash;
  std::optional< T > _value;
};

template< class T >
class merkle_tree
{
  using node_type = merkle_node< T >;

  merkle_tree( std::unique_ptr< node_type > root ):
    _root( std::move( root ) )
  {}

public:
  static std::expected< merkle_tree< T >, error > create( multicodec code, const std::vector< T >& elements )
  {
    // The only thing that should cause an error here is a bad code, test for it first
    if( auto size = multihash::standard_size( code ); size.error() )
      return std::unexpected( size.error() );

    if( !elements.size() )
      return merkle_tree< T >( std::make_unique< node_type >( code ) );

    std::vector< std::unique_ptr< node_type > > nodes;

    for( auto& e: elements )
      nodes.push_back( std::make_unique< node_type >( code, e ) );

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
          next.push_back( std::make_unique< node_type >( code, std::move( left ), std::move( right ) ) );
        }
        else
          next.push_back( std::move( left ) );
      }

      count = next.size();
      nodes = std::move( next );
    }

    return merkle_tree< T >( std::move( nodes.front() ) );
  }

  const std::unique_ptr< node_type >& root()
  {
    return _root;
  }

private:
  std::unique_ptr< node_type > _root;
};

} // namespace koinos::crypto
