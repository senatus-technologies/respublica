// NOLINTBEGIN

#include <gtest/gtest.h>

#include <respublica/crypto/hash.hpp>
#include <respublica/protocol/account.hpp>
#include <respublica/state_db/state_delta.hpp>
#include <respublica/state_db/types.hpp>

respublica::protocol::account make_test_account( std::uint8_t id )
{
  respublica::protocol::account acc{};
  acc[ 0 ] = std::byte{ id };
  return acc;
}

std::shared_ptr< respublica::state_db::state_delta > make_merge_delta( std::vector< std::shared_ptr< respublica::state_db::state_delta > >&& parents )
{
  if( auto merge = respublica::state_db::state_delta::create_delta( respublica::state_db::null_id, std::move( parents ), make_test_account( 1 ), 0, ~0 ); merge )
    return merge.value();
  else
    ADD_FAILURE() << merge.error().message();

  return {};
}

TEST( state_delta, crud )
{
  auto delta = std::make_shared< respublica::state_db::state_delta >();
  ASSERT_TRUE( delta );

  EXPECT_EQ( delta->revision(), 0 );
  delta->set_revision( 1 );
  EXPECT_EQ( delta->revision(), 1 );

  EXPECT_FALSE( delta->complete() );
  EXPECT_EQ( delta->id(), respublica::state_db::state_node_id{} );

  EXPECT_TRUE( delta->root() );
  EXPECT_TRUE( delta->parents().empty() );

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
  EXPECT_EQ( &*parent, &*( child->parents().at( 0 ) ) );

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
  root->mark_complete();

  auto child = root->make_child( { std::byte{ 0x01 } } );

  child->put( std::vector< std::byte >( key_2 ), value_2 );
  child->put( std::vector< std::byte >( key_3 ), value_3 );
  child->put( std::vector< std::byte >( key_4 ), value_4 );
  child->put( std::vector< std::byte >( key_8 ), value_8a );
  child->put( std::vector< std::byte >( key_9 ), value_9a );
  child->put( std::vector< std::byte >( key_10 ), value_10a );
  child->remove( std::vector< std::byte >( key_11 ) );
  child->remove( std::vector< std::byte >( key_12 ) );
  child->mark_complete();

  auto grandchild = child->make_child( { std::byte{ 0x02 } } );

  grandchild->put( std::vector< std::byte >( key_1 ), value_1 );
  grandchild->put( std::vector< std::byte >( key_3 ), value_3a );
  grandchild->remove( std::vector< std::byte >( key_4 ) );
  grandchild->put( std::vector< std::byte >( key_6 ), value_6a );
  grandchild->remove( std::vector< std::byte >( key_7 ) );
  grandchild->put( std::vector< std::byte >( key_9 ), value_9b );
  grandchild->remove( std::vector< std::byte >( key_10 ) );
  grandchild->put( std::vector< std::byte >( key_12 ), value_12a );
  grandchild->mark_complete();

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

TEST( state_delta, dag_basic )
{
  // Test basic DAG structure with multiple parents (merge scenario)
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };
  std::vector< std::byte > key_3{ std::byte{ 0x03 } }, value_3{ std::byte{ 0x30 } };
  std::vector< std::byte > key_4{ std::byte{ 0x04 } }, value_4{ std::byte{ 0x40 } };

  // Set up root with initial values
  root->put( std::vector< std::byte >( key_1 ), value_1 );

  // Create two diverging branches
  auto left  = root->make_child( { std::byte{ 0x01 } } );
  auto right = root->make_child( { std::byte{ 0x02 } } );

  left->put( std::vector< std::byte >( key_2 ), value_2 );
  right->put( std::vector< std::byte >( key_3 ), value_3 );

  // Create merge node with both branches as parents
  auto merge = make_merge_delta( { left, right } );
  ASSERT_TRUE( merge );

  merge->put( std::vector< std::byte >( key_4 ), value_4 );

  EXPECT_FALSE( merge->root() );
  EXPECT_EQ( merge->parents().size(), 2 );

  // Merge should see values from both parents
  if( auto value = merge->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1 ) );
  else
    ADD_FAILURE() << "merge did not return value from root";

  if( auto value = merge->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "merge did not return value from left";

  if( auto value = merge->get( key_3 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_3 ) );
  else
    ADD_FAILURE() << "merge did not return value from right";

  if( auto value = merge->get( key_4 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_4 ) );
  else
    ADD_FAILURE() << "merge did not return its own value";

  // Verify parents don't see merge's changes
  EXPECT_FALSE( left->get( key_4 ) );
  EXPECT_FALSE( right->get( key_4 ) );
}

TEST( state_delta, dag_override )
{
  // Test that modifications in merge node override parent values
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } }, value_1a{ std::byte{ 0x11 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } }, value_2a{ std::byte{ 0x21 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );

  auto left  = root->make_child( { std::byte{ 0x01 } } );
  auto right = root->make_child( { std::byte{ 0x02 } } );

  left->put( std::vector< std::byte >( key_1 ), value_1a );
  right->put( std::vector< std::byte >( key_2 ), value_2 );

  auto merge = make_merge_delta( { left, right } );
  ASSERT_TRUE( merge );

  // Merge should see left's override of key_1
  if( auto value = merge->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1a ) );
  else
    ADD_FAILURE() << "merge did not return overridden value";

  // Now override in merge node itself
  merge->put( std::vector< std::byte >( key_2 ), value_2a );

  if( auto value = merge->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2a ) );
  else
    ADD_FAILURE() << "merge did not return its own override";

  // Parent should still have original value
  if( auto value = right->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "parent value was modified";
}

TEST( state_delta, dag_removal )
{
  // Test removal behavior in DAG with multiple parents
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };
  std::vector< std::byte > key_3{ std::byte{ 0x03 } }, value_3{ std::byte{ 0x30 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );

  auto left  = root->make_child( { std::byte{ 0x01 } } );
  auto right = root->make_child( { std::byte{ 0x02 } } );

  left->put( std::vector< std::byte >( key_2 ), value_2 );
  right->put( std::vector< std::byte >( key_3 ), value_3 );

  auto merge = make_merge_delta( { left, right } );
  ASSERT_TRUE( merge );

  // Remove key from left parent
  merge->remove( std::vector< std::byte >( key_2 ) );

  EXPECT_FALSE( merge->get( key_2 ) );
  EXPECT_TRUE( merge->removed( key_2 ) );

  // Left parent should still have the value
  if( auto value = left->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "removal affected parent";

  // Remove key from root through merge
  merge->remove( std::vector< std::byte >( key_1 ) );

  EXPECT_FALSE( merge->get( key_1 ) );
  EXPECT_TRUE( merge->removed( key_1 ) );

  // Root should still have the value
  if( auto value = root->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1 ) );
  else
    ADD_FAILURE() << "removal affected ancestor";
}

TEST( state_delta, dag_complex_traversal )
{
  // Test more complex DAG structure with multiple merge points
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };
  std::vector< std::byte > key_3{ std::byte{ 0x03 } }, value_3{ std::byte{ 0x30 } };
  std::vector< std::byte > key_4{ std::byte{ 0x04 } }, value_4{ std::byte{ 0x40 } };
  std::vector< std::byte > key_5{ std::byte{ 0x05 } }, value_5{ std::byte{ 0x50 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );

  // Create two branches
  auto branch_a = root->make_child( { std::byte{ 0x0a } } );
  auto branch_b = root->make_child( { std::byte{ 0x0b } } );

  branch_a->put( std::vector< std::byte >( key_2 ), value_2 );
  branch_b->put( std::vector< std::byte >( key_3 ), value_3 );

  // Create sub-branches
  auto sub_a1 = branch_a->make_child( { std::byte{ 0xa1 } } );
  auto sub_a2 = branch_a->make_child( { std::byte{ 0xa2 } } );

  sub_a1->put( std::vector< std::byte >( key_4 ), value_4 );
  sub_a2->put( std::vector< std::byte >( key_5 ), value_5 );

  // Create merge of sub-branches
  auto merge1 = make_merge_delta( { sub_a1, sub_a2 } );
  ASSERT_TRUE( merge1 );

  // Create final merge with branch_b
  auto final_merge = make_merge_delta( { merge1, branch_b } );
  ASSERT_TRUE( final_merge );

  // Verify all values are accessible through complex DAG
  if( auto value = final_merge->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1 ) );
  else
    ADD_FAILURE() << "final_merge did not return value from root";

  if( auto value = final_merge->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "final_merge did not return value from branch_a";

  if( auto value = final_merge->get( key_3 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_3 ) );
  else
    ADD_FAILURE() << "final_merge did not return value from branch_b";

  if( auto value = final_merge->get( key_4 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_4 ) );
  else
    ADD_FAILURE() << "final_merge did not return value from sub_a1";

  if( auto value = final_merge->get( key_5 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_5 ) );
  else
    ADD_FAILURE() << "final_merge did not return value from sub_a2";
}

TEST( state_delta, dag_commit )
{
  // Test committing a DAG structure to root
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };
  std::vector< std::byte > key_3{ std::byte{ 0x03 } }, value_3{ std::byte{ 0x30 } };
  std::vector< std::byte > key_4{ std::byte{ 0x04 } }, value_4{ std::byte{ 0x40 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );
  root->mark_complete();

  // Create diamond structure
  auto left  = root->make_child( { std::byte{ 0x01 } } );
  auto right = root->make_child( { std::byte{ 0x02 } } );

  left->put( std::vector< std::byte >( key_2 ), value_2 );
  left->mark_complete();

  right->put( std::vector< std::byte >( key_3 ), value_3 );
  right->mark_complete();

  auto merge = make_merge_delta( { left, right } );
  ASSERT_TRUE( merge );

  merge->put( std::vector< std::byte >( key_4 ), value_4 );
  merge->mark_complete();

  // Verify all values before commit
  EXPECT_TRUE( merge->get( key_1 ) );
  EXPECT_TRUE( merge->get( key_2 ) );
  EXPECT_TRUE( merge->get( key_3 ) );
  EXPECT_TRUE( merge->get( key_4 ) );

  // Commit the merge node
  merge->commit();

  // After commit, merge should be root
  EXPECT_TRUE( merge->root() );
  EXPECT_TRUE( merge->parents().empty() );

  // All values should still be accessible
  if( auto value = merge->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1 ) );
  else
    ADD_FAILURE() << "commit lost value from root";

  if( auto value = merge->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "commit lost value from left";

  if( auto value = merge->get( key_3 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_3 ) );
  else
    ADD_FAILURE() << "commit lost value from right";

  if( auto value = merge->get( key_4 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_4 ) );
  else
    ADD_FAILURE() << "commit lost merge's own value";
}

TEST( state_delta, dag_three_way_merge )
{
  // Test merge with three parents
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };
  std::vector< std::byte > key_3{ std::byte{ 0x03 } }, value_3{ std::byte{ 0x30 } };
  std::vector< std::byte > key_4{ std::byte{ 0x04 } }, value_4{ std::byte{ 0x40 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );

  auto branch_1 = root->make_child( { std::byte{ 0x01 } } );
  auto branch_2 = root->make_child( { std::byte{ 0x02 } } );
  auto branch_3 = root->make_child( { std::byte{ 0x03 } } );

  branch_1->put( std::vector< std::byte >( key_2 ), value_2 );
  branch_2->put( std::vector< std::byte >( key_3 ), value_3 );
  branch_3->put( std::vector< std::byte >( key_4 ), value_4 );

  auto merge = make_merge_delta( { branch_1, branch_2, branch_3 } );
  ASSERT_TRUE( merge );

  EXPECT_EQ( merge->parents().size(), 3 );

  // Verify merge can access all parent values
  if( auto value = merge->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1 ) );
  else
    ADD_FAILURE() << "merge did not return value from root";

  if( auto value = merge->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "merge did not return value from branch1";

  if( auto value = merge->get( key_3 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_3 ) );
  else
    ADD_FAILURE() << "merge did not return value from branch2";

  if( auto value = merge->get( key_4 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_4 ) );
  else
    ADD_FAILURE() << "merge did not return value from branch3";
}

TEST( state_delta, dag_branch_removal_visibility )
{
  // Test that when a branch removes a key, merge nodes see the removal
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );
  root->put( std::vector< std::byte >( key_2 ), value_2 );

  // Create two branches
  auto branch1 = root->make_child( { std::byte{ 0x01 } } );
  auto branch2 = root->make_child( { std::byte{ 0x02 } } );

  // Branch1 removes key_1 from root
  branch1->remove( std::vector< std::byte >( key_1 ) );

  // Verify branch1 sees the removal
  EXPECT_FALSE( branch1->get( key_1 ) );
  EXPECT_TRUE( branch1->removed( key_1 ) );

  // Branch2 doesn't touch key_1, so should still see it from root
  branch2->mark_complete();
  if( auto value = branch2->get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1 ) );
  else
    ADD_FAILURE() << "branch2 did not see value from root";

  // Create merge of both branches
  auto merge = make_merge_delta( { branch1, branch2 } );
  ASSERT_TRUE( merge );

  // This is the key test: merge should see the removal from branch1
  EXPECT_FALSE( merge->get( key_1 ) );

  // Merge should still see key_2 (not removed by anyone)
  if( auto value = merge->get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "merge did not see key_2 from root";
}

TEST( state_delta, mark_complete )
{
  auto delta = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };

  delta->put( std::vector< std::byte >( key_1 ), value_1 );
  delta->put( std::vector< std::byte >( key_2 ), value_2 );

  delta->mark_complete();

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

TEST( state_delta, conflict_write_write_same_key )
{
  // Test write-write conflict: two branches modify the same key
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );

  auto branch1 = root->make_child();
  auto branch2 = root->make_child();

  std::vector< std::byte > value_a{ std::byte{ 0xAA } };
  std::vector< std::byte > value_b{ std::byte{ 0xBB } };

  // Both branches modify the same key
  branch1->put( std::vector< std::byte >( key_1 ), value_a );
  branch2->put( std::vector< std::byte >( key_1 ), value_b );

  // Should conflict
  EXPECT_TRUE( branch1->has_conflict( *branch2 ) );
  EXPECT_TRUE( branch2->has_conflict( *branch1 ) );
}

TEST( state_delta, conflict_write_write_same_value )
{
  // Test write-write conflict: two branches write the same value (still a conflict)
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } };

  auto branch1 = root->make_child();
  auto branch2 = root->make_child();

  std::vector< std::byte > value_same{ std::byte{ 0xAA } };

  // Both branches write the same value to the same key
  branch1->put( std::vector< std::byte >( key_1 ), value_same );
  branch2->put( std::vector< std::byte >( key_1 ), value_same );

  // Should still conflict (same key written)
  EXPECT_TRUE( branch1->has_conflict( *branch2 ) );
  EXPECT_TRUE( branch2->has_conflict( *branch1 ) );
}

TEST( state_delta, conflict_write_remove )
{
  // Test write-write conflict: one branch modifies, another removes
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );

  auto branch1 = root->make_child();
  auto branch2 = root->make_child();

  std::vector< std::byte > value_new{ std::byte{ 0xAA } };

  // Branch1 modifies the key
  branch1->put( std::vector< std::byte >( key_1 ), value_new );
  // Branch2 removes the key
  branch2->remove( std::vector< std::byte >( key_1 ) );

  // Should conflict
  EXPECT_TRUE( branch1->has_conflict( *branch2 ) );
  EXPECT_TRUE( branch2->has_conflict( *branch1 ) );
}

TEST( state_delta, conflict_read_after_write )
{
  // Test read-after-write conflict: one branch writes, another reads
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );

  auto branch1 = root->make_child();
  auto branch2 = root->make_child();

  std::vector< std::byte > value_new{ std::byte{ 0xAA } };

  // Branch1 modifies the key
  branch1->put( std::vector< std::byte >( key_1 ), value_new );

  // Branch2 reads the key (gets old value from root)
  auto read_value = branch2->get( key_1 );
  EXPECT_TRUE( read_value.has_value() );

  // Should conflict (branch2 read what branch1 modified)
  EXPECT_TRUE( branch1->has_conflict( *branch2 ) );
  EXPECT_TRUE( branch2->has_conflict( *branch1 ) );
}

TEST( state_delta, conflict_read_after_remove )
{
  // Test read-after-write conflict: one branch removes, another reads
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );

  auto branch1 = root->make_child();
  auto branch2 = root->make_child();

  // Branch1 removes the key
  branch1->remove( std::vector< std::byte >( key_1 ) );

  // Branch2 reads the key (gets value from root)
  auto read_value = branch2->get( key_1 );
  EXPECT_TRUE( read_value.has_value() );

  // Should conflict (branch2 read what branch1 removed)
  EXPECT_TRUE( branch1->has_conflict( *branch2 ) );
  EXPECT_TRUE( branch2->has_conflict( *branch1 ) );
}

TEST( state_delta, conflict_read_nonexistent )
{
  // Test read-after-write conflict: one branch writes new key, another reads (non-existent)
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } };

  auto branch1 = root->make_child();
  auto branch2 = root->make_child();

  std::vector< std::byte > value_new{ std::byte{ 0xAA } };

  // Branch1 creates a new key
  branch1->put( std::vector< std::byte >( key_1 ), value_new );

  // Branch2 reads the key (doesn't exist, returns nullopt)
  auto read_value = branch2->get( key_1 );
  EXPECT_FALSE( read_value.has_value() );

  // Should conflict (branch2 read non-existent key that branch1 created)
  EXPECT_TRUE( branch1->has_conflict( *branch2 ) );
  EXPECT_TRUE( branch2->has_conflict( *branch1 ) );
}

TEST( state_delta, no_conflict_different_keys )
{
  // Test no conflict: branches modify different keys
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );
  root->put( std::vector< std::byte >( key_2 ), value_2 );

  auto branch1 = root->make_child();
  auto branch2 = root->make_child();

  std::vector< std::byte > value_a{ std::byte{ 0xAA } };
  std::vector< std::byte > value_b{ std::byte{ 0xBB } };

  // Branch1 modifies key_1, branch2 modifies key_2
  branch1->put( std::vector< std::byte >( key_1 ), value_a );
  branch2->put( std::vector< std::byte >( key_2 ), value_b );

  // Should not conflict (different keys)
  EXPECT_FALSE( branch1->has_conflict( *branch2 ) );
  EXPECT_FALSE( branch2->has_conflict( *branch1 ) );
}

TEST( state_delta, no_conflict_common_ancestor )
{
  // Test no conflict: nodes with common ancestor writing same key
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };

  // Root writes key_1
  root->put( std::vector< std::byte >( key_1 ), value_1 );

  auto branch1 = root->make_child();
  auto branch2 = root->make_child();

  // Neither branch modifies key_1, just reads it
  auto read1 = branch1->get( key_1 );
  auto read2 = branch2->get( key_1 );

  EXPECT_TRUE( read1.has_value() );
  EXPECT_TRUE( read2.has_value() );

  // Should not conflict (both just read from common ancestor)
  EXPECT_FALSE( branch1->has_conflict( *branch2 ) );
  EXPECT_FALSE( branch2->has_conflict( *branch1 ) );
}

TEST( state_delta, conflict_transitive )
{
  // Test transitive conflict: descendants of conflicting nodes also conflict
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );

  auto branch1 = root->make_child();
  auto branch2 = root->make_child();

  std::vector< std::byte > value_a{ std::byte{ 0xAA } };
  std::vector< std::byte > value_b{ std::byte{ 0xBB } };

  // Both branches modify the same key
  branch1->put( std::vector< std::byte >( key_1 ), value_a );
  branch2->put( std::vector< std::byte >( key_1 ), value_b );

  // Create descendants
  auto child1 = branch1->make_child();
  auto child2 = branch2->make_child();

  // Descendants should also conflict (inherit parent conflicts)
  EXPECT_TRUE( child1->has_conflict( *child2 ) );
  EXPECT_TRUE( child2->has_conflict( *child1 ) );
  EXPECT_TRUE( child1->has_conflict( *branch2 ) );
  EXPECT_TRUE( branch1->has_conflict( *child2 ) );
}

TEST( state_delta, conflict_diamond_pattern )
{
  // Test conflict in diamond DAG: merge nodes can conflict
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );
  root->put( std::vector< std::byte >( key_2 ), value_2 );

  auto left  = root->make_child();
  auto right = root->make_child();

  std::vector< std::byte > value_a{ std::byte{ 0xAA } };

  // Left branch modifies key_1
  left->put( std::vector< std::byte >( key_1 ), value_a );

  // Right branch modifies key_2
  right->put( std::vector< std::byte >( key_2 ), value_a );

  // Create two merge nodes that both merge left and right
  auto merge1 = make_merge_delta( { left, right } );
  ASSERT_TRUE( merge1 );

  auto merge2 = make_merge_delta( { left, right } );
  ASSERT_TRUE( merge2 );

  std::vector< std::byte > key_3{ std::byte{ 0x03 } };

  // Merge1 writes a new key
  merge1->put( std::vector< std::byte >( key_3 ), value_a );

  // Merge2 reads that key (doesn't exist yet)
  auto read_value = merge2->get( key_3 );
  EXPECT_FALSE( read_value.has_value() );

  // Merge nodes should conflict
  EXPECT_TRUE( merge1->has_conflict( *merge2 ) );
  EXPECT_TRUE( merge2->has_conflict( *merge1 ) );
}

TEST( state_delta, no_conflict_self )
{
  // Test that a node doesn't conflict with itself
  auto root = std::make_shared< respublica::state_db::state_delta >();

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };

  root->put( std::vector< std::byte >( key_1 ), value_1 );

  auto branch = root->make_child();

  std::vector< std::byte > value_a{ std::byte{ 0xAA } };
  branch->put( std::vector< std::byte >( key_1 ), value_a );

  // Node should not conflict with itself
  EXPECT_FALSE( branch->has_conflict( *branch ) );
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
  delta->mark_complete();

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

  child->mark_complete();
  EXPECT_TRUE( std::ranges::equal( child->merkle_root(), respublica::crypto::merkle_root( merkle_leafs ) ) );
}

// NOLINTEND
