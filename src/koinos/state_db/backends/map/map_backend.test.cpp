#include <gtest/gtest.h>
#include <koinos/state_db/backends/map/map_backend.hpp>

TEST( map_backend, crud )
{
  koinos::state_db::backends::map::map_backend backend;

  EXPECT_EQ( backend.size(), 0 );

  EXPECT_FALSE( backend.get( { std::byte{ 0x01 } } ) );

  EXPECT_EQ( backend.put( { std::byte{ 0x01 } }, { std::byte{ 0x10 } } ), 2 );
  EXPECT_EQ( backend.size(), 1 );
  if( auto value = backend.get( { std::byte{ 0x01 } } ); value )
    EXPECT_TRUE( std::ranges::equal( *value, std::vector< std::byte >{ std::byte{ 0x10 } } ) );
  else
    ADD_FAILURE() << "backend did not return a value";

  EXPECT_EQ( backend.put( { std::byte{ 0x02 } }, { std::byte{ 0x20 }, std::byte{ 0x21 } } ), 3 );
  EXPECT_EQ( backend.size(), 2 );
  if( auto value = backend.get( { std::byte{ 0x02 } } ); value )
    EXPECT_TRUE( std::ranges::equal( *value, std::vector< std::byte >{ std::byte{ 0x20 }, std::byte{ 0x21 } } ) );
  else
    ADD_FAILURE() << "backend did not return a value";

  EXPECT_EQ( backend.put( { std::byte{ 0x01 } }, { std::byte{ 0x10 }, std::byte{ 0x11 }, std::byte{ 0x12 } } ), 2 );
  EXPECT_EQ( backend.size(), 2 );
  if( auto value = backend.get( { std::byte{ 0x01 } } ); value )
    EXPECT_TRUE( std::ranges::equal( *value, std::vector< std::byte >{ std::byte{ 0x10 }, std::byte{ 0x11 }, std::byte{ 0x12 } } ) );
  else
    ADD_FAILURE() << "backend did not return a value";

  std::vector< std::byte > value{ std::byte{ 0x30 } };
  EXPECT_EQ( backend.put( { std::byte{ 0x03 } }, std::span< const std::byte >( value ) ), 2 );
  EXPECT_EQ( backend.size(), 3 );
  if( auto value = backend.get( { std::byte{ 0x03 } } ); value )
    EXPECT_TRUE( std::ranges::equal( *value, std::vector< std::byte >{ std::byte{ 0x30 } } ) );
  else
    ADD_FAILURE() << "backend did not return a value";

  EXPECT_EQ( backend.put( { std::byte{ 0x01 } }, { std::byte{ 0x10 }, std::byte{ 0x11 } } ), -1 );
  EXPECT_EQ( backend.size(), 3 );

  EXPECT_EQ( backend.remove( { std::byte{ 0x01 } } ), -3 );
  EXPECT_EQ( backend.size(), 2 );
  EXPECT_FALSE( backend.get( { std::byte{ 0x01 } } ) );

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
    if( auto value = clone->get( { std::byte{ 0x02 } } ); value )
      EXPECT_TRUE( std::ranges::equal( *value, std::vector< std::byte >{ std::byte{ 0x20 }, std::byte{ 0x21 } } ) );
    else
      ADD_FAILURE() << "cloned backend did not return a value";

    if( auto value = clone->get( { std::byte{ 0x03 } } ); value )
      EXPECT_TRUE( std::ranges::equal( *value, std::vector< std::byte >{ std::byte{ 0x30 } } ) );
    else
      ADD_FAILURE() << "cloned backend did not return a value";

    EXPECT_EQ( clone->remove( { std::byte{ 0x02 } } ), -3 );
    EXPECT_EQ( clone->size(), 1 );
    EXPECT_EQ( backend.size(), 2 );
  }
  else
    ADD_FAILURE() << "clone did not return a valid pointer";

  backend.clear();
  EXPECT_EQ( backend.size(), 0 );
  EXPECT_FALSE( backend.get( { std::byte{ 0x01 } } ) );
  EXPECT_FALSE( backend.get( { std::byte{ 0x02 } } ) );
  EXPECT_FALSE( backend.get( { std::byte{ 0x03 } } ) );
}

TEST( map_backend, iteration )
{
  koinos::state_db::backends::map::map_backend backend;

  backend.put( { std::byte{ 0x02 } }, { std::byte{ 0x20 } } );
  backend.put( { std::byte{ 0x01 } }, { std::byte{ 0x10 } } );
  backend.put( { std::byte{ 0x03 } }, { std::byte{ 0x30 } } );
  EXPECT_EQ( backend.size(), 3 );

  auto itr = backend.begin();
  ASSERT_NE( itr, backend.end() );
  EXPECT_TRUE( std::ranges::equal( itr->first, std::vector< std::byte >{ std::byte{ 0x01 } } ) );
  EXPECT_TRUE( std::ranges::equal( itr->second, std::vector< std::byte >{ std::byte{ 0x10 } } ) );

  ++itr;
  ASSERT_NE( itr, backend.end() );
  EXPECT_TRUE( std::ranges::equal( itr->first, std::vector< std::byte >{ std::byte{ 0x02 } } ) );
  EXPECT_TRUE( std::ranges::equal( itr->second, std::vector< std::byte >{ std::byte{ 0x20 } } ) );

  --itr;
  ASSERT_NE( itr, backend.end() );
  EXPECT_TRUE( std::ranges::equal( itr->first, std::vector< std::byte >{ std::byte{ 0x01 } } ) );
  EXPECT_TRUE( std::ranges::equal( itr->second, std::vector< std::byte >{ std::byte{ 0x10 } } ) );

  ++itr;
  ++itr;
  ASSERT_NE( itr, backend.end() );
  EXPECT_TRUE( std::ranges::equal( itr->first, std::vector< std::byte >{ std::byte{ 0x03 } } ) );
  EXPECT_TRUE( std::ranges::equal( itr->second, std::vector< std::byte >{ std::byte{ 0x30 } } ) );

  ++itr;
  EXPECT_EQ( itr, backend.end() );

  --itr;
  ASSERT_NE( itr, backend.end() );
  auto pair = itr.release();
  EXPECT_EQ( backend.size(), 2 );
  EXPECT_TRUE( std::ranges::equal( pair.first, std::vector< std::byte >{ std::byte{ 0x03 } } ) );
  EXPECT_TRUE( std::ranges::equal( pair.second, std::vector< std::byte >{ std::byte{ 0x30 } } ) );

  itr = backend.begin();
  pair = itr.release();
  EXPECT_EQ( backend.size(), 1 );
  EXPECT_TRUE( std::ranges::equal( pair.first, std::vector< std::byte >{ std::byte{ 0x01 } } ) );
  EXPECT_TRUE( std::ranges::equal( pair.second, std::vector< std::byte >{ std::byte{ 0x10 } } ) );
}