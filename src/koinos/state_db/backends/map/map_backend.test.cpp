#include <gtest/gtest.h>
#include <koinos/state_db/backends/map/map_backend.hpp>

TEST( map_backend, crud )
{
  koinos::state_db::backends::map::map_backend backend;

  EXPECT_EQ( backend.size(), 0 );

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x01 } };
  EXPECT_FALSE( backend.get( key_1 ) );

  EXPECT_EQ( backend.put( std::vector< std::byte >( key_1 ), value_1 ), key_1.size() + value_1.size() );
  EXPECT_EQ( backend.size(), 1 );
  if( auto value = backend.get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1 ) );
  else
    ADD_FAILURE() << "backend did not return a value";

  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 }, std::byte{ 0x21 } };
  EXPECT_EQ( backend.put( std::vector< std::byte >( key_2 ), value_2 ), key_2.size() + value_2.size() );
  EXPECT_EQ( backend.size(), 2 );
  if( auto value = backend.get( key_2 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
  else
    ADD_FAILURE() << "backend did not return a value";

  std::vector< std::byte > value_1a{ std::byte{ 0x10 }, std::byte{ 0x11 }, std::byte{ 0x12 } };
  EXPECT_EQ( backend.put( std::vector< std::byte >( key_1 ), value_1a ), value_1a.size() - value_1.size() );
  EXPECT_EQ( backend.size(), 2 );
  if( auto value = backend.get( key_1 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_1a ) );
  else
    ADD_FAILURE() << "backend did not return a value";

  // Test putting an r-value value.
  std::vector< std::byte > key_3{ std::byte{ 0x03 } }, value_3{ std::byte{ 0x30 } };
  EXPECT_EQ( backend.put( std::vector< std::byte >( key_3 ), std::vector< std::byte >( value_3 ) ),
             key_3.size() + value_3.size() );
  EXPECT_EQ( backend.size(), 3 );
  if( auto value = backend.get( key_3 ); value )
    EXPECT_TRUE( std::ranges::equal( *value, value_3 ) );
  else
    ADD_FAILURE() << "backend did not return a value";

  const std::vector< std::byte > value_1b{ std::byte{ 0x10 }, std::byte{ 0x11 } };
  EXPECT_EQ( backend.put( std::vector< std::byte >( key_1 ), value_1b ), value_1b.size() - value_1a.size() );
  EXPECT_EQ( backend.size(), 3 );

  EXPECT_EQ( backend.remove( std::vector< std::byte >( key_1 ) ), -1 * ( key_1.size() + value_1b.size() ) );
  EXPECT_EQ( backend.size(), 2 );
  EXPECT_FALSE( backend.get( key_1 ) );

  EXPECT_EQ( backend.remove( { std::byte{ 0x04 } } ), 0 );
  EXPECT_EQ( backend.size(), 2 );

  // These are all nops, testing for coverage
  EXPECT_NO_THROW( backend.start_write_batch() );
  EXPECT_NO_THROW( backend.end_write_batch() );
  EXPECT_NO_THROW( backend.store_metadata() );

  EXPECT_EQ( backend.size(), 2 );

  if( auto clone = backend.clone(); clone )
  {
    EXPECT_EQ( clone->size(), 2 );
    if( auto value = clone->get( key_2 ); value )
      EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
    else
      ADD_FAILURE() << "cloned backend did not return a value";

    if( auto value = clone->get( key_2 ); value )
      EXPECT_TRUE( std::ranges::equal( *value, value_2 ) );
    else
      ADD_FAILURE() << "cloned backend did not return a value";

    EXPECT_EQ( clone->remove( std::vector< std::byte >( key_2 ) ), -3 );
    EXPECT_EQ( clone->size(), 1 );
    EXPECT_EQ( backend.size(), 2 );
  }
  else
    ADD_FAILURE() << "clone did not return a valid pointer";

  backend.clear();
  EXPECT_EQ( backend.size(), 0 );
  EXPECT_FALSE( backend.get( key_1 ) );
  EXPECT_FALSE( backend.get( key_2 ) );
  EXPECT_FALSE( backend.get( key_3 ) );
}

TEST( map_backend, iteration )
{
  koinos::state_db::backends::map::map_backend backend;

  std::vector< std::byte > key_1{ std::byte{ 0x01 } }, value_1{ std::byte{ 0x10 } };
  std::vector< std::byte > key_2{ std::byte{ 0x02 } }, value_2{ std::byte{ 0x20 } };
  std::vector< std::byte > key_3{ std::byte{ 0x03 } }, value_3{ std::byte{ 0x30 } };

  // Purposefully add them out of order to ensure ordering is coincidental
  backend.put( std::vector< std::byte >( key_2 ), value_2 );
  backend.put( std::vector< std::byte >( key_1 ), value_1 );
  backend.put( std::vector< std::byte >( key_3 ), value_3 );
  EXPECT_EQ( backend.size(), 3 );

  auto itr = backend.begin();
  ASSERT_NE( itr, backend.end() );
  EXPECT_TRUE( std::ranges::equal( itr->first, key_1 ) );
  EXPECT_TRUE( std::ranges::equal( itr->second, value_1 ) );

  ++itr;
  ASSERT_NE( itr, backend.end() );
  EXPECT_TRUE( std::ranges::equal( itr->first, key_2 ) );
  EXPECT_TRUE( std::ranges::equal( itr->second, value_2 ) );

  --itr;
  ASSERT_NE( itr, backend.end() );
  EXPECT_TRUE( std::ranges::equal( itr->first, key_1 ) );
  EXPECT_TRUE( std::ranges::equal( itr->second, value_1 ) );

  ++itr;
  ++itr;
  ASSERT_NE( itr, backend.end() );
  EXPECT_TRUE( std::ranges::equal( itr->first, key_3 ) );
  EXPECT_TRUE( std::ranges::equal( itr->second, value_3 ) );

  ++itr;
  EXPECT_EQ( itr, backend.end() );

  --itr;
  ASSERT_NE( itr, backend.end() );
  auto pair = itr.release();
  EXPECT_EQ( backend.size(), 2 );
  EXPECT_TRUE( std::ranges::equal( pair.first, key_3 ) );
  EXPECT_TRUE( std::ranges::equal( pair.second, value_3 ) );

  itr  = backend.begin();
  pair = itr.release();
  EXPECT_EQ( backend.size(), 1 );
  EXPECT_TRUE( std::ranges::equal( pair.first, key_1 ) );
  EXPECT_TRUE( std::ranges::equal( pair.second, value_1 ) );
}
