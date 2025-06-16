// NOLINTBEGIN

#include <gtest/gtest.h>
#include <koinos/state_db/backends/backend.hpp>

class test_backend final: public koinos::state_db::backends::abstract_backend
{
public:
  std::uint64_t _size = 0;

  koinos::state_db::backends::iterator begin() override
  {
    return koinos::state_db::backends::iterator( {} );
  }

  koinos::state_db::backends::iterator end() override
  {
    return koinos::state_db::backends::iterator( {} );
  }

  std::int64_t put( std::vector< std::byte >&&, std::span< const std::byte > ) override
  {
    return 0;
  }

  std::int64_t put( std::vector< std::byte >&&, std::vector< std::byte >&& ) override
  {
    return 0;
  }

  std::optional< std::span< const std::byte > > get( const std::vector< std::byte >& ) const override
  {
    return {};
  }

  std::int64_t remove( const std::vector< std::byte >& ) override
  {
    return 0;
  }

  void clear() override {}

  std::uint64_t size() const override
  {
    return _size;
  }

  void start_write_batch() override {}

  void end_write_batch() override {}

  void store_metadata() override {}

  std::shared_ptr< abstract_backend > clone() const override
  {
    return {};
  }
};

TEST( backend, base )
{
  test_backend backend;

  // Test empty()
  EXPECT_TRUE( backend.empty() );

  backend._size = 1;
  EXPECT_FALSE( backend.empty() );

  // Test revision()
  EXPECT_EQ( backend.revision(), 0 );

  backend.set_revision( 1 );
  EXPECT_EQ( backend.revision(), 1 );

  // Test id()
  EXPECT_EQ( backend.id(), koinos::state_db::state_node_id{} );

  backend.set_id( koinos::state_db::state_node_id{ std::byte{ 0x01 } } );
  EXPECT_EQ( backend.id(), koinos::state_db::state_node_id{ std::byte{ 0x01 } } );

  // Test merkle_root()
  EXPECT_EQ( backend.merkle_root(), koinos::state_db::digest{} );

  backend.set_merkle_root( koinos::state_db::digest{ std::byte{ 0x01 } } );
  EXPECT_EQ( backend.merkle_root(), koinos::state_db::digest{ std::byte{ 0x01 } } );
}

// NOLINTEND
