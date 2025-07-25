// NOLINTBEGIN

#include <gtest/gtest.h>

#include <respublica/crypto/hash.hpp>
#include <respublica/state_db/state_delta.hpp>

TEST( state_delta, crud )
{
  auto delta = std::make_shared< respublica::state_db::state_delta >();
  ASSERT_TRUE( delta );

  EXPECT_EQ( delta->revision(), 0 );
  delta->set_revision( 1 );
  EXPECT_EQ( delta->revision(), 1 );

  EXPECT_FALSE( delta->final() );
  EXPECT_EQ( delta->id(), respublica::state_db::state_node_id{} );

  EXPECT_TRUE( delta->root() );
  EXPECT_FALSE( delta->parent() );

  EXPECT_THROW( delta->merkle_root(), std::runtime_error );

  EXPECT_FALSE( delta->get( { std::byte{ 0x01 } } ) );

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  EXPECT_EQ( delta->put( std::vector< std::byte >( key_1 ), value_1 ), key_1.size() + value_1.size() );
  if( auto value = delta->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1 ) );
  else
    ADD_FAILURE() << "delta did not return a value";

  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 }, std::byte{ 0x21 } };
  EXPECT_EQ( delta->put( std::vector< std::byte >( key_2 ), value_2 ), key_2.size() + value_2.size() );
  if( auto value = delta->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "delta did not return a value";

  std::vector< std::byte > value_1a{ std::byte{ 0x10 }, std::byte{ 0x11 }, std::byte{ 0x12 } };
  EXPECT_EQ( delta->put( std::vector< std::byte >( key_1 ), value_1a ), value_1a.size() - value_1.size() );
  if( auto value = delta->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1a ) );
  else
    ADD_FAILURE() << "delta did not return a value";

  std::vector< std::byte > key_3{ std::byte{ 0x03 } }, value_3{ std::byte{ 0x30 } };
  EXPECT_EQ( delta->put( std::vector< std::byte >( key_3 ), value_3 ), key_3.size() + value_3.size() );
  if( auto value = delta->get( key_3 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_3 ) );
  else
    ADD_FAILURE() << "delta did not return a value";

  std::vector< std::byte > value_1b{ std::byte{ 0x10 }, std::byte{ 0x11 } };
  EXPECT_EQ( delta->put( std::vector< std::byte >( key_1 ), value_1b ), value_1b.size() - value_1a.size() );

  if( auto clone = delta->clone(); clone )
  {
    EXPECT_EQ( clone->revision(), 1 );
    EXPECT_EQ( clone->id(), respublica::state_db::state_node_id{} );

    if( auto value = clone->get( key_1 ); value )
      EXPECT_TRUE( std::ranges::equal( *value, value_1b ) );
    else
      ADD_FAILURE() << "clone did not return a value";

    if( auto value = clone->get( key_2 ); value )
      EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
    else
      ADD_FAILURE() << "clone did not return a value";

    if( auto value = clone->get( key_3 ); value )
      EXPECT_TRUE( std::ranges::equal( *value, value_3 ) );
    else
      ADD_FAILURE() << "clone did not return a value";

    std::vector< std::byte > key_4{ std::byte{ 0x04 } }, value_4{ std::byte{ 0x40 } };
    clone->put( std::vector< std::byte >( key_4 ), value_4 );
    EXPECT_FALSE( delta->get( { std::byte{ 0x04 } } ) );

    clone->remove( std::vector< std::byte >( key_1 ) );
    EXPECT_TRUE( delta->get( key_1 ) );
  }
  else
    ADD_FAILURE() << "clone did not return a valid pointer";

  EXPECT_EQ( delta->remove( std::vector< std::byte >( key_1 ) ), -1 * ( key_1.size() + value_1b.size() ) );
  EXPECT_TRUE( delta->removed( key_1 ) );
  EXPECT_FALSE( delta->get( key_1 ) );
  EXPECT_EQ( delta->remove( std::vector< std::byte >( key_1 ) ), 0 );

  std::vector< std::byte > key_4{ std::byte{ 0x04 } };
  EXPECT_FALSE( delta->removed( key_4 ) );
  EXPECT_EQ( delta->remove( std::vector< std::byte >( key_4 ) ), 0 );
  EXPECT_FALSE( delta->removed( key_4 ) );

  delta->clear();
  EXPECT_FALSE( delta->get( key_1 ) );
  EXPECT_FALSE( delta->get( key_2 ) );
  EXPECT_FALSE( delta->get( key_3 ) );
  EXPECT_FALSE( delta->removed( key_1 ) );
  EXPECT_FALSE( delta->removed( key_2 ) );
  EXPECT_FALSE( delta->removed( key_3 ) );
}

TEST( state_delta, children )
{
  auto parent = std::make_shared< respublica::state_db::state_delta >();
  ASSERT_TRUE( parent );

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  parent->put( std::vector< std::byte >( key_1 ), value_1 );

  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };
  parent->put( std::vector< std::byte >( key_2 ), value_2 );

  std::vector< std::byte > key_3{ std::byte{ 0x03 } }, value_3{ std::byte{ 0x30 } };
  parent->put( std::vector< std::byte >( key_3 ), value_3 );

  auto child = parent->make_child( { std::byte{ 0x01 } } );
  ASSERT_TRUE( child );

  EXPECT_EQ( child->id(), respublica::state_db::digest{ std::byte{ 0x01 } } );
  EXPECT_EQ( child->parent_id(), respublica::state_db::digest{} );
  EXPECT_EQ( &*parent, &*( child->parent() ) );

  std::vector< std::byte > key_4{ std::byte{ 0x04 } }, value_4{ std::byte{ 0x40 } };
  EXPECT_EQ( child->put( std::vector< std::byte >( key_4 ), value_4 ), key_4.size() + value_4.size() );
  EXPECT_FALSE( parent->get( key_4 ) );
  if( auto value = child->get( key_4 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_4 ) );
  else
    ADD_FAILURE() << "child did not return a value";

  std::vector< std::byte > value_1a{ std::byte{ 0x10 }, std::byte{ 0x11 } };
  EXPECT_EQ( child->put( std::vector< std::byte >( key_1 ), value_1a ), value_1a.size() - value_1.size() );
  if( auto value = parent->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1 ) );
  else
    ADD_FAILURE() << "parent did not return a value";

  if( auto value = child->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1a ) );
  else
    ADD_FAILURE() << "child did not return a value";

  EXPECT_EQ( child->remove( std::vector< std::byte >( key_2 ) ), -1 * ( key_2.size() + value_2.size() ) );
  if( auto value = parent->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "parent did not return a value";

  EXPECT_FALSE( child->get( key_2 ) );
  EXPECT_TRUE( child->removed( key_2 ) );

  std::vector< std::byte > value_3a{ std::byte{ 0x30 }, std::byte{ 0x31 }, std::byte{ 0x32 } };
  EXPECT_EQ( child->put( std::vector< std::byte >( key_3 ), value_3a ), value_3a.size() - value_3.size() );
  EXPECT_EQ( child->remove( std::vector< std::byte >( key_3 ) ), -1 * ( key_3.size() + value_3a.size() ) );
  EXPECT_TRUE( child->removed( key_3 ) );
  EXPECT_FALSE( parent->removed( key_3 ) );

  child->squash();

  if( auto value = parent->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1a ) );
  else
    ADD_FAILURE() << "parent did not return a value";

  EXPECT_FALSE( parent->get( key_2 ) );
  EXPECT_FALSE( parent->removed( key_2 ) );

  EXPECT_FALSE( parent->get( key_3 ) );
  EXPECT_FALSE( parent->removed( key_3 ) );

  if( auto value = parent->get( key_4 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_4 ) );
  else
    ADD_FAILURE() << "parent did not return a value";

  child           = parent->make_child( { std::byte{ 0x01 } } );
  auto grandchild = child->make_child( { std::byte{ 0x02 } } );

  grandchild->remove( std::vector< std::byte >( key_1 ) );
  EXPECT_FALSE( grandchild->get( key_1 ) );
  EXPECT_TRUE( grandchild->removed( key_1 ) );
  EXPECT_TRUE( child->get( key_1 ) );
  EXPECT_FALSE( child->removed( key_1 ) );
  EXPECT_TRUE( parent->get( key_1 ) );
  EXPECT_FALSE( parent->removed( key_1 ) );

  grandchild->squash();
  EXPECT_FALSE( child->get( key_1 ) );
  EXPECT_TRUE( child->removed( key_1 ) );
  EXPECT_TRUE( parent->get( key_1 ) );
  EXPECT_FALSE( parent->removed( key_1 ) );
}

TEST( state_delta, commit )
{
  /**
   * We are testing a commit to root. This squashes everything in to a single delta.
   *
   * Each delta there are two operations, put and remove.
   * Implicitly, there is a third which is to leave it alone.
   * We can test all permutations using three deltas.
   *
   * For completeness, here are all of the permutations:
   *
   *      0   1   2
   *    +---+---+---+
   *  0 |   |   |   |
   *  1 |   |   | P |
   *  2 |   |   | R |
   *  3 |   | P |   |
   *  4 |   | P | P |
   *  5 |   | P | R |
   *  6 |   | R |   |
   *  7 |   | R | P |
   *  8 |   | R | R |
   *  9 | P |   |   |
   * 10 | P |   | P |
   * 11 | P |   | R |
   * 12 | P | P |   |
   * 13 | P | P | P |
   * 14 | P | P | R |
   * 15 | P | R |   |
   * 16 | P | R | P |
   * 17 | P | R | R |
   *    +---+---+---+
   *
   * We can ignore all cases where there is a remove after no puts or double removes.
   * These are cases 2, 6, 7, 8, and 17.
   *
   * The final table is:
   *
   *      0   1   2
   *    +---+---+---+
   *  0 |   |   |   |
   *  1 |   |   | P |
   *  2 |   | P |   |
   *  3 |   | P | P |
   *  4 |   | P | R |
   *  5 | P |   |   |
   *  6 | P |   | P |
   *  7 | P |   | R |
   *  8 | P | P |   |
   *  9 | P | P | P |
   * 10 | P | P | R |
   * 11 | P | R |   |
   * 12 | P | R | P |
   *    +---+---+---+
   */

  std::vector< std::byte > key_0{};
  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };
  std::vector< std::byte > key_3{ std::byte{ 0x03 } }, value_3{ std::byte{ 0x30 } }, value_3a{ std::byte{ 0x31 } };
  std::vector< std::byte > key_4{ std::byte{ 0x04 } }, value_4{ std::byte{ 0x40 } };
  std::vector< std::byte > key_5{ std::byte{ 0x05 } }, value_5{ std::byte{ 0x50 } };
  std::vector< std::byte > key_6{ std::byte{ 0x06 } }, value_6{ std::byte{ 0x60 } }, value_6a{ std::byte{ 0x61 } };
  std::vector< std::byte > key_7{ std::byte{ 0x07 } }, value_7{ std::byte{ 0x70 } };
  std::vector< std::byte > key_8{ std::byte{ 0x08 } }, value_8{ std::byte{ 0x80 } }, value_8a{ std::byte{ 0x81 } };
  std::vector< std::byte > key_9{ std::byte{ 0x09 } }, value_9{ std::byte{ 0x90 } }, value_9a{ std::byte{ 0x91 } },
    value_9b{ std::byte{ 0x91 } };
  std::vector< std::byte > key_10{ std::byte{ 0x0a } }, value_10{ std::byte{ 0xa0 } }, value_10a{ std::byte{ 0xa1 } };
  std::vector< std::byte > key_11{ std::byte{ 0x0b } }, value_11{ std::byte{ 0xb0 } };
  std::vector< std::byte > key_12{ std::byte{ 0x0c } }, value_12{ std::byte{ 0xc0 } }, value_12a{ std::byte{ 0xc1 } };

  auto root = std::make_shared< respublica::state_db::state_delta >();

  root->put( std::vector< std::byte >( key_5 ), value_5 );
  root->put( std::vector< std::byte >( key_6 ), value_6 );
  root->put( std::vector< std::byte >( key_7 ), value_7 );
  root->put( std::vector< std::byte >( key_8 ), value_8 );
  root->put( std::vector< std::byte >( key_9 ), value_9 );
  root->put( std::vector< std::byte >( key_10 ), value_10 );
  root->put( std::vector< std::byte >( key_11 ), value_11 );
  root->put( std::vector< std::byte >( key_12 ), value_12 );
  root->finalize();

  auto child = root->make_child( { std::byte{ 0x01 } } );

  child->put( std::vector< std::byte >( key_2 ), value_2 );
  child->put( std::vector< std::byte >( key_3 ), value_3 );
  child->put( std::vector< std::byte >( key_4 ), value_4 );
  child->put( std::vector< std::byte >( key_8 ), value_8a );
  child->put( std::vector< std::byte >( key_9 ), value_9a );
  child->put( std::vector< std::byte >( key_10 ), value_10a );
  child->remove( std::vector< std::byte >( key_11 ) );
  child->remove( std::vector< std::byte >( key_12 ) );
  child->finalize();

  auto grandchild = child->make_child( { std::byte{ 0x02 } } );

  grandchild->put( std::vector< std::byte >( key_1 ), value_1 );
  grandchild->put( std::vector< std::byte >( key_3 ), value_3a );
  grandchild->remove( std::vector< std::byte >( key_4 ) );
  grandchild->put( std::vector< std::byte >( key_6 ), value_6a );
  grandchild->remove( std::vector< std::byte >( key_7 ) );
  grandchild->put( std::vector< std::byte >( key_9 ), value_9b );
  grandchild->remove( std::vector< std::byte >( key_10 ) );
  grandchild->put( std::vector< std::byte >( key_12 ), value_12a );
  grandchild->finalize();

  EXPECT_FALSE( grandchild->root() );

  EXPECT_FALSE( grandchild->get( key_0 ) );
  EXPECT_FALSE( grandchild->removed( key_0 ) );

  if( auto value = grandchild->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1 ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  if( auto value = grandchild->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  if( auto value = grandchild->get( key_3 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_3a ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  EXPECT_FALSE( grandchild->get( key_4 ) );
  EXPECT_TRUE( grandchild->removed( key_4 ) );

  if( auto value = grandchild->get( key_5 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_5 ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  if( auto value = grandchild->get( key_6 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_6a ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  EXPECT_FALSE( grandchild->get( key_7 ) );
  EXPECT_TRUE( grandchild->removed( key_7 ) );

  if( auto value = grandchild->get( key_8 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_8a ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  if( auto value = grandchild->get( key_9 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_9a ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  EXPECT_FALSE( grandchild->get( key_10 ) );
  EXPECT_TRUE( grandchild->removed( key_10 ) );

  EXPECT_FALSE( grandchild->get( key_11 ) );
  EXPECT_FALSE( grandchild->removed( key_11 ) );

  if( auto value = grandchild->get( key_12 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_12a ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  grandchild->commit();

  EXPECT_FALSE( grandchild->get( key_0 ) );
  EXPECT_FALSE( grandchild->removed( key_0 ) );

  if( auto value = grandchild->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1 ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  if( auto value = grandchild->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  if( auto value = grandchild->get( key_3 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_3a ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  EXPECT_FALSE( grandchild->get( key_4 ) );
  EXPECT_FALSE( grandchild->removed( key_4 ) );

  if( auto value = grandchild->get( key_5 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_5 ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  if( auto value = grandchild->get( key_6 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_6a ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  EXPECT_FALSE( grandchild->get( key_7 ) );
  EXPECT_FALSE( grandchild->removed( key_7 ) );

  if( auto value = grandchild->get( key_8 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_8a ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  if( auto value = grandchild->get( key_9 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_9a ) );
  else
    ADD_FAILURE() << "grandchild did not return value";

  EXPECT_FALSE( grandchild->get( key_10 ) );
  EXPECT_FALSE( grandchild->removed( key_10 ) );

  EXPECT_FALSE( grandchild->get( key_11 ) );
  EXPECT_FALSE( grandchild->removed( key_11 ) );

  if( auto value = grandchild->get( key_12 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_12a ) );
  else
    ADD_FAILURE() << "grandchild did not return value";
}

TEST( state_delta, finalize )
{
  auto delta = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };

  delta->put( std::vector< std::byte >( key_1 ), value_1 );
  delta->put( std::vector< std::byte >( key_2 ), value_2 );

  delta->finalize();

  std::vector< std::byte > value_1a{ std::byte{ 0x11 } };
  EXPECT_THROW( delta->put( std::vector< std::byte >( key_1 ), value_1a ), std::runtime_error );
  if( auto value = delta->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1 ) );
  else
    ADD_FAILURE() << "delta did not return value";

  EXPECT_THROW( delta->remove( std::vector< std::byte >( key_2 ) ), std::runtime_error );
  if( auto value = delta->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "delta did not return value";
}

TEST( state_delta, merkle_root )
{
  auto delta = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };
  std::vector< std::byte > key_3{ std::byte{ 0x03 } }, value_3{ std::byte{ 0x30 } };

  delta->put( std::vector< std::byte >( key_1 ), value_1 );
  delta->put( std::vector< std::byte >( key_2 ), value_2 );
  delta->put( std::vector< std::byte >( key_3 ), value_3 );

  std::vector< std::span< const std::byte > > merkle_leafs;
  merkle_leafs.emplace_back( key_1 );
  merkle_leafs.emplace_back( value_1 );
  merkle_leafs.emplace_back( key_2 );
  merkle_leafs.emplace_back( value_2 );
  merkle_leafs.emplace_back( key_3 );
  merkle_leafs.emplace_back( value_3 );

  EXPECT_THROW( delta->merkle_root(), std::runtime_error );
  delta->finalize();

  EXPECT_TRUE( std::ranges::equal( delta->merkle_root(), respublica::crypto::merkle_root( merkle_leafs ) ) );

  auto child = delta->make_child( { std::byte{ 0x01 } } );

  std::vector< std::byte > value_1a{ std::byte{ 0x11 } };
  std::vector< std::byte > key_4{ std::byte{ 0x04 } }, value_4{ std::byte{ 0x40 } };

  child->put( std::vector< std::byte >( key_1 ), value_1a );
  child->remove( std::vector< std::byte >( key_2 ) );
  child->put( std::vector< std::byte >( key_4 ), value_4 );

  merkle_leafs.clear();
  merkle_leafs.emplace_back( key_1 );
  merkle_leafs.emplace_back( value_1a );
  merkle_leafs.emplace_back( key_2 );
  merkle_leafs.emplace_back();
  merkle_leafs.emplace_back( key_4 );
  merkle_leafs.emplace_back( value_4 );

  child->finalize();
  EXPECT_TRUE( std::ranges::equal( child->merkle_root(), respublica::crypto::merkle_root( merkle_leafs ) ) );
}

// NOLINTEND
