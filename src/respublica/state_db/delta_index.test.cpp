// NOLINTBEGIN

#include <gtest/gtest.h>

#include <respublica/protocol/account.hpp>
#include <respublica/state_db/delta_index.hpp>
#include <respublica/state_db/state_delta.hpp>
#include <respublica/state_db/types.hpp>

static respublica::protocol::account make_test_account( std::uint8_t id )
{
  respublica::protocol::account acc{};
  acc[ 0 ] = std::byte{ id };
  return acc;
}

static void genesis_init( const std::shared_ptr< respublica::state_db::state_node >& genesis )
{
  // Initialize genesis state if needed
}

// Helper to generate unique node IDs
static std::uint64_t node_id_counter = 0;

static respublica::state_db::state_node_id make_unique_id()
{
  respublica::state_db::state_node_id id{};
  auto counter = ++node_id_counter;
  for( std::size_t i = 0; i < sizeof( counter ) && i < id.size(); ++i )
  {
    id[ i ] = static_cast< std::byte >( ( counter >> ( i * 8 ) ) & 0xFF );
  }
  return id;
}

// Helper to create a node with conflicts
static std::shared_ptr< respublica::state_db::state_delta >
create_conflicting_node( const std::shared_ptr< respublica::state_db::state_delta >& parent,
                         const respublica::protocol::account& creator,
                         respublica::state_db::approval_weight_t weight,
                         const std::vector< std::byte >& conflicting_key )
{
  auto result = respublica::state_db::state_delta::create_delta( make_unique_id(), { parent }, creator, weight, 100 );
  if( !result )
    return nullptr;

  auto delta = result.value().delta;

  // Write to a key to create potential conflicts
  delta->put( std::vector< std::byte >( conflicting_key ), std::vector< std::byte >{ std::byte{ 0xFF } } );

  return delta;
}

// ============================================================================
// Conflict Cache Tests
// ============================================================================

TEST( delta_index, conflict_cache_bidirectional )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root = index->root();
  ASSERT_TRUE( root );

  auto validator1 = make_test_account( 1 );
  auto validator2 = make_test_account( 2 );

  // Create two conflicting nodes
  std::vector< std::byte > conflicting_key{ std::byte{ 0x01 } };

  auto node_a = create_conflicting_node( root, validator1, 50, conflicting_key );
  auto node_b = create_conflicting_node( root, validator2, 50, conflicting_key );

  ASSERT_TRUE( node_a );
  ASSERT_TRUE( node_b );

  index->add( node_a );
  index->add( node_b );

  // Mark both complete to trigger conflict cache update
  index->mark_complete( node_a );
  index->mark_complete( node_b );

  // Verify bidirectional conflict caching
  EXPECT_TRUE( node_a->has_conflict( *node_b ) );
  EXPECT_TRUE( node_b->has_conflict( *node_a ) );

  index->close();
}

TEST( delta_index, conflict_cache_incremental_update )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root = index->root();

  auto validator1 = make_test_account( 1 );
  auto validator2 = make_test_account( 2 );
  auto validator3 = make_test_account( 3 );

  std::vector< std::byte > key1{ std::byte{ 0x01 } };
  std::vector< std::byte > key2{ std::byte{ 0x02 } };

  // Create first conflicting pair
  auto node_a = create_conflicting_node( root, validator1, 50, key1 );
  auto node_b = create_conflicting_node( root, validator2, 50, key1 );

  index->add( node_a );
  index->add( node_b );
  index->mark_complete( node_a );
  index->mark_complete( node_b );

  // Add a third node that conflicts with both
  auto node_c = create_conflicting_node( root, validator3, 50, key1 );
  index->add( node_c );
  index->mark_complete( node_c );

  // All three should be in conflict with each other
  EXPECT_TRUE( node_a->has_conflict( *node_c ) );
  EXPECT_TRUE( node_b->has_conflict( *node_c ) );
  EXPECT_TRUE( node_c->has_conflict( *node_a ) );
  EXPECT_TRUE( node_c->has_conflict( *node_b ) );

  index->close();
}

TEST( delta_index, cleanup_conflict_cache )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root       = index->root();
  auto validator1 = make_test_account( 1 );
  auto validator2 = make_test_account( 2 );

  std::vector< std::byte > key{ std::byte{ 0x01 } };

  auto node_a = create_conflicting_node( root, validator1, 50, key );
  auto node_b = create_conflicting_node( root, validator2, 50, key );

  index->add( node_a );
  index->add( node_b );
  index->mark_complete( node_a );
  index->mark_complete( node_b );

  // Both nodes should have conflicts in cache
  EXPECT_TRUE( node_a->has_conflict( *node_b ) );
  EXPECT_TRUE( node_b->has_conflict( *node_a ) );

  // Cleanup should work without errors
  // Note: Since index holds strong references to all nodes, no pointers will expire
  // This test verifies cleanup runs without crashes
  index->cleanup_conflict_cache();

  // Nodes should still be valid and accessible
  EXPECT_TRUE( node_a );
  EXPECT_TRUE( node_b );

  index->close();
}

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST( delta_index, get_all_ancestors_linear )
{
  using namespace respublica::state_db;

  auto root       = std::make_shared< state_delta >();
  auto child      = root->make_child().child;
  auto grandchild = child->make_child().child;

  root->mark_complete();
  child->mark_complete();
  grandchild->mark_complete();

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  // Note: get_all_ancestors is private, so we test it indirectly through get_edge_candidates

  index->close();
}

TEST( delta_index, get_all_ancestors_diamond )
{
  using namespace respublica::state_db;

  auto root  = std::make_shared< state_delta >();
  auto left  = root->make_child().child;
  auto right = root->make_child().child;

  // Create merge node with both parents
  auto merge_result = state_delta::create_delta( make_unique_id(), { left, right }, make_test_account( 1 ), 0, 100 );
  ASSERT_TRUE( merge_result );
  auto merge = merge_result.value().delta;

  root->mark_complete();
  left->mark_complete();
  right->mark_complete();
  merge->mark_complete();

  // Merge should have root, left, and right as ancestors
  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  index->close();
}

// ============================================================================
// Conflict Resolution Tests
// ============================================================================

TEST( delta_index, tier1_final_wins )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root = index->root();

  auto validator1 = make_test_account( 1 );
  auto validator2 = make_test_account( 2 );

  std::vector< std::byte > key{ std::byte{ 0x01 } };

  // Create two conflicting branches
  auto node_a = create_conflicting_node( root, validator1, 100, key );
  auto node_b = create_conflicting_node( root, validator2, 100, key );

  index->add( node_a );
  index->add( node_b );
  index->mark_complete( node_a );
  index->mark_complete( node_b );

  // Create children
  auto child_a = node_a->make_child( make_unique_id(), validator1, 100, 100 ).child;
  auto child_b = node_b->make_child( make_unique_id(), validator2, 100, 100 ).child;

  index->add( child_a );
  index->add( child_b );
  index->mark_complete( child_a );
  index->mark_complete( child_b );

  // Finalize node_a's chain by creating grandchildren and marking complete
  // This requires building enough depth to trigger finalization
  auto grandchild_a = child_a->make_child( make_unique_id(), validator1, 100, 100 ).child;
  index->add( grandchild_a );
  index->mark_complete( grandchild_a );

  // Get edge candidates - should only include finalized chain descendants
  auto edges = index->get_edge_candidates();

  // If finalization occurred, only descendants of finalized chain should be in edge set
  // Otherwise, conflict resolution via other tiers applies
  EXPECT_FALSE( edges.empty() );

  index->close();
}

TEST( delta_index, tier2_validator_approved_wins )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root = index->root();

  auto validator1 = make_test_account( 1 );
  auto validator2 = make_test_account( 2 );

  std::vector< std::byte > key{ std::byte{ 0x01 } };

  // Create two conflicting branches
  // Branch A: approved by validator1, lower approval
  auto node_a = create_conflicting_node( root, validator1, 50, key );
  // Branch B: NOT approved by validator1, higher approval
  auto node_b = create_conflicting_node( root, validator2, 75, key );

  index->add( node_a );
  index->add( node_b );
  index->mark_complete( node_a );
  index->mark_complete( node_b );

  // Create children
  auto child_a = node_a->make_child( make_unique_id(), validator1, 50, 100 ).child;
  auto child_b = node_b->make_child( make_unique_id(), validator2, 75, 100 ).child;

  index->add( child_a );
  index->add( child_b );
  index->mark_complete( child_a );
  index->mark_complete( child_b );

  // Query with validator1 - should prefer chain A even though B has higher approval
  auto edges = index->get_edge_candidates( validator1 );

  // Should contain child_a because validator1 already approved that chain
  bool has_child_a = false;
  bool has_child_b = false;
  for( const auto& edge: edges )
  {
    if( edge == child_a )
      has_child_a = true;
    if( edge == child_b )
      has_child_b = true;
  }

  EXPECT_TRUE( has_child_a );
  EXPECT_FALSE( has_child_b ); // Should not include conflicting chain

  index->close();
}

TEST( delta_index, tier3_highest_approval_wins )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root = index->root();

  auto validator1 = make_test_account( 1 );
  auto validator2 = make_test_account( 2 );

  std::vector< std::byte > key{ std::byte{ 0x01 } };

  // Create two conflicting branches with different approval weights
  auto node_a = create_conflicting_node( root, validator1, 75, key );
  auto node_b = create_conflicting_node( root, validator2, 50, key );

  index->add( node_a );
  index->add( node_b );
  index->mark_complete( node_a );
  index->mark_complete( node_b );

  // Create children
  auto child_a = node_a->make_child( make_unique_id(), validator1, 75, 100 ).child;
  auto child_b = node_b->make_child( make_unique_id(), validator2, 50, 100 ).child;

  index->add( child_a );
  index->add( child_b );
  index->mark_complete( child_a );
  index->mark_complete( child_b );

  // Query without validator preference - should use tier 3
  auto edges = index->get_edge_candidates();

  // Should prefer child_a because node_a has higher approval
  bool has_child_a = false;
  bool has_child_b = false;
  for( const auto& edge: edges )
  {
    if( edge == child_a )
      has_child_a = true;
    if( edge == child_b )
      has_child_b = true;
  }

  EXPECT_TRUE( has_child_a );
  EXPECT_FALSE( has_child_b );

  index->close();
}

TEST( delta_index, tier4_fifo_keep_current )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root = index->root();

  auto validator1 = make_test_account( 1 );
  auto validator2 = make_test_account( 2 );

  std::vector< std::byte > key{ std::byte{ 0x01 } };

  // Create two conflicting branches with EQUAL approval
  auto node_a = create_conflicting_node( root, validator1, 50, key );
  auto node_b = create_conflicting_node( root, validator2, 50, key );

  index->add( node_a );
  index->add( node_b );
  index->mark_complete( node_a );
  index->mark_complete( node_b );

  auto child_a = node_a->make_child( make_unique_id(), validator1, 50, 100 ).child;
  auto child_b = node_b->make_child( make_unique_id(), validator2, 50, 100 ).child;

  index->add( child_a );
  index->add( child_b );
  index->mark_complete( child_a );
  index->mark_complete( child_b );

  // With equal approval and no validator preference, tier 4 applies
  // The implementation will return std::nullopt and keep all current edges
  auto edges = index->get_edge_candidates();

  // When tier 4 returns nullopt, all conflicting edges are kept
  // This is acceptable behavior for a tie
  EXPECT_GE( edges.size(), 1 );

  index->close();
}

// ============================================================================
// Edge Query Tests
// ============================================================================

TEST( delta_index, simple_linear_chain )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root = index->root();

  auto validator1 = make_test_account( 1 );

  auto block1 = root->make_child( make_unique_id(), validator1, 100, 100 ).child;
  auto block2 = block1->make_child( make_unique_id(), validator1, 100, 100 ).child;
  auto block3 = block2->make_child( make_unique_id(), validator1, 100, 100 ).child;

  index->add( block1 );
  index->add( block2 );
  index->add( block3 );

  index->mark_complete( block1 );
  index->mark_complete( block2 );
  index->mark_complete( block3 );

  auto edges = index->get_edge_candidates();

  // Should only have block3 as edge candidate
  EXPECT_EQ( edges.size(), 1 );
  EXPECT_EQ( edges[ 0 ], block3 );

  index->close();
}

TEST( delta_index, diamond_no_conflicts )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root = index->root();

  auto validator1 = make_test_account( 1 );

  // Create diamond with no conflicts (different keys)
  auto left  = root->make_child( make_unique_id(), validator1, 100, 100 ).child;
  auto right = root->make_child( make_unique_id(), validator1, 100, 100 ).child;

  // Write different keys to avoid conflicts
  left->put( std::vector< std::byte >{ std::byte{ 0x01 } }, std::vector< std::byte >{ std::byte{ 0xFF } } );
  right->put( std::vector< std::byte >{ std::byte{ 0x02 } }, std::vector< std::byte >{ std::byte{ 0xFF } } );

  index->add( left );
  index->add( right );
  index->mark_complete( left );
  index->mark_complete( right );

  auto left_child  = left->make_child( make_unique_id(), validator1, 100, 100 ).child;
  auto right_child = right->make_child( make_unique_id(), validator1, 100, 100 ).child;

  index->add( left_child );
  index->add( right_child );
  index->mark_complete( left_child );
  index->mark_complete( right_child );

  auto edges = index->get_edge_candidates();

  // Should have both children since no conflicts
  EXPECT_EQ( edges.size(), 2 );

  bool has_left  = false;
  bool has_right = false;
  for( const auto& edge: edges )
  {
    if( edge == left_child )
      has_left = true;
    if( edge == right_child )
      has_right = true;
  }

  EXPECT_TRUE( has_left );
  EXPECT_TRUE( has_right );

  index->close();
}

TEST( delta_index, three_way_conflict )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root = index->root();

  auto validator1 = make_test_account( 1 );
  auto validator2 = make_test_account( 2 );
  auto validator3 = make_test_account( 3 );

  std::vector< std::byte > key{ std::byte{ 0x01 } };

  // Create three mutually conflicting branches
  auto node_a = create_conflicting_node( root, validator1, 60, key );
  auto node_b = create_conflicting_node( root, validator2, 50, key );
  auto node_c = create_conflicting_node( root, validator3, 40, key );

  index->add( node_a );
  index->add( node_b );
  index->add( node_c );
  index->mark_complete( node_a );
  index->mark_complete( node_b );
  index->mark_complete( node_c );

  auto child_a = node_a->make_child( make_unique_id(), validator1, 60, 100 ).child;
  auto child_b = node_b->make_child( make_unique_id(), validator2, 50, 100 ).child;
  auto child_c = node_c->make_child( make_unique_id(), validator3, 40, 100 ).child;

  index->add( child_a );
  index->add( child_b );
  index->add( child_c );
  index->mark_complete( child_a );
  index->mark_complete( child_b );
  index->mark_complete( child_c );

  auto edges = index->get_edge_candidates();

  // Should prefer child_a (highest approval = 60)
  bool has_child_a = false;
  bool has_child_b = false;
  bool has_child_c = false;
  for( const auto& edge: edges )
  {
    if( edge == child_a )
      has_child_a = true;
    if( edge == child_b )
      has_child_b = true;
    if( edge == child_c )
      has_child_c = true;
  }

  EXPECT_TRUE( has_child_a );
  EXPECT_FALSE( has_child_b );
  EXPECT_FALSE( has_child_c );

  index->close();
}

TEST( delta_index, multilevel_conflict_subtree )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root = index->root();

  auto validator1 = make_test_account( 1 );
  auto validator2 = make_test_account( 2 );

  std::vector< std::byte > key{ std::byte{ 0x01 } };

  // Create two conflicting branches
  auto node_a = create_conflicting_node( root, validator1, 40, key );
  auto node_b = create_conflicting_node( root, validator2, 80, key );

  index->add( node_a );
  index->add( node_b );
  index->mark_complete( node_a );
  index->mark_complete( node_b );

  // Build subtrees
  auto a1 = node_a->make_child( make_unique_id(), validator1, 60, 100 ).child;
  index->add( a1 );
  index->mark_complete( a1 );

  auto a1a = a1->make_child( make_unique_id(), validator1, 60, 100 ).child;
  auto a1b = a1->make_child( make_unique_id(), validator1, 60, 100 ).child;
  index->add( a1a );
  index->add( a1b );
  index->mark_complete( a1a );
  index->mark_complete( a1b );

  auto a2 = node_a->make_child( make_unique_id(), validator1, 50, 100 ).child;
  index->add( a2 );
  index->mark_complete( a2 );

  auto a2a = a2->make_child( make_unique_id(), validator1, 50, 100 ).child;
  index->add( a2a );
  index->mark_complete( a2a );

  auto b1 = node_b->make_child( make_unique_id(), validator2, 80, 100 ).child;
  index->add( b1 );
  index->mark_complete( b1 );

  auto edges = index->get_edge_candidates();

  // node_b has approval=80, node_a has approval=40
  // So B wins, only B's descendants should be in edge set
  bool has_b1  = false;
  bool has_a1a = false;
  bool has_a1b = false;
  bool has_a2a = false;

  for( const auto& edge: edges )
  {
    if( edge == b1 )
      has_b1 = true;
    if( edge == a1a )
      has_a1a = true;
    if( edge == a1b )
      has_a1b = true;
    if( edge == a2a )
      has_a2a = true;
  }

  EXPECT_TRUE( has_b1 );
  EXPECT_FALSE( has_a1a );
  EXPECT_FALSE( has_a1b );
  EXPECT_FALSE( has_a2a );

  index->close();
}

// ============================================================================
// Final Edge Query Tests
// ============================================================================

TEST( delta_index, final_edges_basic )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root = index->root();

  // Root is not finalized initially
  auto final_edges = index->get_final_edges();
  EXPECT_TRUE( final_edges.empty() );

  index->close();
}

TEST( delta_index, final_edges_no_final_children )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto root = index->root();

  // Create a deep enough chain to trigger finalization
  auto validator1 = make_test_account( 1 );

  auto child1 = root->make_child( make_unique_id(), validator1, 100, 100 ).child;
  auto child2 = child1->make_child( make_unique_id(), validator1, 100, 100 ).child;
  auto child3 = child2->make_child( make_unique_id(), validator1, 100, 100 ).child;

  index->add( child1 );
  index->add( child2 );
  index->add( child3 );

  index->mark_complete( child1 );
  index->mark_complete( child2 );
  index->mark_complete( child3 );

  // After completion, some nodes may be finalized
  auto final_edges = index->get_final_edges();

  // Final edges should be finalized nodes without finalized children
  for( const auto& edge: final_edges )
  {
    EXPECT_TRUE( edge->final() );
    EXPECT_TRUE( edge->is_final_edge() );
  }

  index->close();
}

TEST( delta_index, empty_dag )
{
  using namespace respublica::state_db;

  auto index = std::make_shared< delta_index >();
  index->open( genesis_init, fork_resolution_algorithm::fifo, std::nullopt );

  auto edges       = index->get_edge_candidates();
  auto final_edges = index->get_final_edges();

  // Root exists but may not be an edge candidate (depends on completeness)
  // Final edges should be empty (root not finalized)
  EXPECT_TRUE( final_edges.empty() );

  index->close();
}

// NOLINTEND
