# DAG Edge Set Queries - Design Document

## 1. Motivation

### 1.1 Problem Statement

In a DAG-based blockchain consensus system, validators need to quickly identify which nodes to build upon when proposing new blocks. The "edge" of the DAG represents approved, non-conflicting nodes without children—these are the ideal candidates for new block proposals.

Previously, finding these edges required expensive graph traversal and on-the-fly conflict detection. For each query:
- Traverse the entire DAG to find childless nodes
- Perform pairwise conflict checks (O(n²) worst case)
- Apply conflict resolution heuristics
- No caching or optimization possible

### 1.2 Use Cases

**Validator Block Proposal**: When a validator wants to create a new block, they query the edge set to determine which nodes to use as parents. The returned set must be:
- Conflict-free (no double-spends or read-write conflicts)
- Optimized for the validator's interests (prefer chains they've already approved)
- Consistent with consensus rules (finalized nodes take precedence)

**Epoch Detection**: Finalized nodes without finalized children represent epoch boundaries. Efficiently querying these enables:
- Checkpoint creation
- State pruning
- Light client sync points

### 1.3 Solution Overview

This design introduces two public query functions with efficient conflict resolution:

```cpp
// Returns non-conflicting edge candidates for block proposal
std::vector<state_delta_ptr> get_edge_candidates(
    const std::optional<protocol::account>& validator_account = std::nullopt
) const;

// Returns finalized nodes without finalized children (epoch boundaries)
std::vector<state_delta_ptr> get_final_edges() const;
```

Key innovations:
- **Conflict caching** with weak pointers for automatic memory management
- **Incremental updates** on node completion (O(n) instead of O(n²))
- **Lazy cleanup** that can run on utility threads
- **4-tier conflict resolution** heuristic (final > validator-approved > most-approved > FIFO)

## 2. Background: Existing Infrastructure

### 2.1 DAG Structure

Nodes in the DAG are represented by `state_delta` objects with:
- **Parent pointers** (strong `shared_ptr`) - ancestors in the DAG
- **Child pointers** (weak `weak_ptr`) - descendants in the DAG
- **Approval tracking** - map of validators to approval weights
- **Finalization status** - whether the node is irreversibly committed

### 2.2 Multi-Index Support

The `delta_index` uses Boost Multi-Index with five indices:
- `by_id` - O(log n) lookup by node ID
- `by_final` - O(log n) query for finalized nodes
- `by_is_edge_candidate` - O(log n) query for complete, non-final, childless nodes
- `by_is_final_edge` - O(1) query for final nodes without final children
- `by_approval_weight` - O(log n) range queries by approval weight

### 2.3 Impacted Node Tracking

All state-mutating operations return sets of "impacted" nodes whose indexed properties changed:
- Creating a child impacts the parent (gained a child, no longer an edge)
- Marking complete impacts ancestors (received approval propagation)
- Finalization impacts both the node and its children (edge status changes)

The `update_node()` helper triggers Boost Multi-Index reindexing for all impacted nodes.

### 2.4 Conflict Detection

The existing `has_conflict()` method on `state_delta` detects:
- **Write-write conflicts**: Both nodes modify the same key
- **Read-after-write conflicts**: One node reads a key another modifies

Conflict detection uses graph traversal to find common ancestors and check for overlapping state modifications.

## 3. Conflict Resolution Requirements

### 3.1 The Double-Spend Problem

Consider this scenario:

```
root (balance[A] = 100)
├── block1: transfer A→B (50 coins)
│   └── block2 (validator1 approved)
└── block3: transfer A→C (50 coins)  [CONFLICTS with block1]
    └── block4 (validator3 approved)
```

Both `block1` and `block2` read and modify `balance[A]`, creating a conflict. Due to network latency, blocks have been built on both conflicting transactions.

**Question**: Which blocks should be in the edge set?
- `[block2, block4]`? This includes both conflicting chains.
- `[block2]`? If we prefer block1.
- `[block4]`? If we prefer block2.

The answer depends on conflict resolution policy.

### 3.2 Four-Tier Conflict Resolution Heuristic

When conflicts are detected, apply these rules in order:

#### Tier 1: Finalized Nodes Win

Once a node is finalized, all conflicting non-final nodes (and their descendants) are excluded from the edge set.

```
root
├── A (FINAL, conflicts with B)
│   └── A1 (edge candidate)
└── B (not final, conflicts with A)
    └── B1 (edge candidate)

Edge set = [A1]  // B is excluded because it conflicts with final node A
```

**Rationale**: Finalization represents irreversible consensus. Conflicting chains cannot both finalize, so the finalized chain takes absolute precedence.

#### Tier 2: Validator-Approved Nodes Win

If a validator has already approved a node in a conflict set, prefer that chain for consistency.

```
root
├── A (validator1 approved, conflicts with B)
│   └── A1 (edge candidate)
└── B (not validator1 approved, conflicts with A)
    └── B1 (edge candidate)

Query: get_edge_candidates(validator1)
Edge set = [A1]  // Validator1 already committed to chain A
```

**Rationale**: Validators should build on chains they've already approved. Switching between conflicting chains would be inconsistent and could appear malicious.

#### Tier 3: Highest Approval Weight Wins

Prefer the node with the most accumulated approval from the validator set.

```
root
├── A (total_approval = 75, conflicts with B)
│   └── A1 (edge candidate)
└── B (total_approval = 50, conflicts with A)
    └── B1 (edge candidate)

Edge set = [A1]  // A has more approval weight
```

**Rationale**: Democratic weight indicates which chain has more support from the validator set.

#### Tier 4: FIFO (Keep Current Edge Set)

If tiers 1-3 don't produce a clear winner, keep the current edge set unchanged.

```
root
├── A (total_approval = 50, conflicts with B, FIRST SEEN)
│   └── A1 (edge candidate) [currently in edge set]
└── B (total_approval = 50, conflicts with A, seen later)
    └── B1 (edge candidate)

Edge set = [A1]  // Keep first-seen chain
```

**Rationale**: This provides stability. Validators who saw A first will keep building on A until a tier 1-3 rule changes the winner. This is implicit FIFO: the first-seen chain gets "locked in" until overtaken.

### 3.3 Conflict Subtree Example

This example demonstrates how conflicts between parent nodes affect edge candidates in child subtrees:

```
root
├── A (approval=50)
│   ├── A1 (edge candidate)
│   └── A2 (edge candidate)
└── B (approval=60, conflicts with A)
    ├── B1 (edge candidate)
    └── B2 (edge candidate)
```

**Initial state**: A was first-seen, so edge set = `[A1, A2]`

**After B2 is added**: B's approval increases to 60, overtaking A (tier 3 applies)

**New edge set**: `[B1, B2]`

**Key insight**: The conflict is between A and B (parents), but the edge set contains their children (A1, A2, B1, B2). When resolving conflicts, we:
1. Identify the conflicting roots (A vs B)
2. Apply the 4-tier heuristic to choose a winner (B wins)
3. Return all edge candidates descended from the winner (B1, B2)

## 4. Detailed Design

### 4.1 Data Structures

Add to `delta_index` class:

```cpp
private:
  // Conflict tracking with weak pointers for automatic cleanup
  struct conflict_info
  {
    std::unordered_set<std::weak_ptr<state_delta>,
                       std::owner_less<std::weak_ptr<state_delta>>> conflicts_with;
  };

  mutable std::unordered_map<std::weak_ptr<state_delta>,
                              conflict_info,
                              std::owner_less<std::weak_ptr<state_delta>>> _conflict_cache;
```

**Design rationale**:
- **Weak pointers**: Nodes can be deallocated without manual cache invalidation
- **`std::owner_less`**: Required for weak_ptr as map key (compares ownership, not pointer value)
- **`mutable`**: Cache updates are logically const operations (don't change DAG structure)

### 4.2 Conflict Cache Management

#### 4.2.1 Incremental Updates on Node Completion

```cpp
private:
  void update_conflict_cache_for_node(const state_delta_ptr& node);
  void cache_conflict_if_exists(const state_delta_ptr& node1,
                                const state_delta_ptr& node2);
```

Modified `mark_complete()`:

```cpp
void delta_index::mark_complete(const state_delta_ptr& ptr)
{
  if (!is_open())
    throw std::runtime_error("database is not open");

  auto impacted_nodes = ptr->mark_complete();

  for (const auto& node: impacted_nodes)
    update_node(node);

  // Incrementally update conflict cache for the newly completed node
  update_conflict_cache_for_node(ptr);
}
```

**Algorithm: `update_conflict_cache_for_node(node)`**
```
1. Iterate through all nodes in _index
2. For each other_node:
   - Call cache_conflict_if_exists(node, other_node)
```

**Algorithm: `cache_conflict_if_exists(node1, node2)`**
```
1. If node1->has_conflict(*node2):
   - Add weak_ptr(node2) to _conflict_cache[weak_ptr(node1)].conflicts_with
   - Add weak_ptr(node1) to _conflict_cache[weak_ptr(node2)].conflicts_with
```

**Complexity**: O(n) per `mark_complete()` call, where n is the total number of nodes in the index.

#### 4.2.2 Lazy Cleanup

```cpp
public:
  // Clean up expired weak pointers and remove finalized nodes from conflict cache
  // Safe to call from utility thread
  void cleanup_conflict_cache();
```

**Algorithm: `cleanup_conflict_cache()`**
```
1. Iterate through _conflict_cache entries:
   - If key.expired()
     - Remove entry
   - Else for each conflict in conflicts_with:
     - If conflict.expired()
       - Remove conflict from set

2. Remove entries with empty conflict sets
```

**Design rationale**:
- Called periodically from a utility thread (not on critical path)
- Removes expired weak pointers (nodes deallocated)
- O(c) where c is cache size

#### 4.2.3 Full Rebuild (for initialization or manual refresh)

```cpp
private:
  void rebuild_conflict_cache() const;
```

**Algorithm: `rebuild_conflict_cache()`**
```
1. Clear _conflict_cache
2. Get all nodes from _index
3. For each pair of nodes (i, j) where i < j:
   - Call cache_conflict_if_exists(nodes[i], nodes[j])
```

**Complexity**: O(n²) where n is number of nodes. Should only be called on startup or after major state changes.

### 4.3 Helper Functions

#### 4.3.1 Lock and Filter Conflicts

```cpp
private:
  std::unordered_set<state_delta_ptr> lock_and_filter_conflicts(
    const std::weak_ptr<state_delta>& node
  ) const;
```

**Algorithm**:
```
1. Look up node in _conflict_cache
2. If not found, return empty set
3. For each weak_ptr in conflicts_with:
   - Lock the weak_ptr
   - If lock succeeds, add to result set
4. Return result set
```

**Purpose**: Convert weak_ptr conflict set to shared_ptr set, filtering expired pointers.

#### 4.3.2 Get All Ancestors

```cpp
private:
  std::unordered_set<state_delta_ptr> get_all_ancestors(
    const state_delta_ptr& node
  ) const;
```

**Algorithm**:
```
1. Initialize ancestors = {}
2. Initialize queue = { node }
3. Initialize visited = { node }

4. While queue not empty:
   - current = queue.pop()
   - For each parent in current->parents():
     - If parent not in visited:
       - Add to ancestors
       - Add to queue
       - Add to visited

5. Return ancestors
```

**Complexity**: O(a) where a is number of ancestors (bounded by DAG depth).

#### 4.3.3 Get Conflict Closure

```cpp
private:
  std::unordered_set<state_delta_ptr> get_conflict_closure(
    const state_delta_ptr& node,
    const std::unordered_set<state_delta_ptr>& candidate_pool
  ) const;
```

**Algorithm**:
```
1. Initialize closure = { node }
2. Initialize queue = { node }

3. While queue not empty:
   - current = queue.pop()
   - conflicts = lock_and_filter_conflicts(current)

   - For each conflict in conflicts:
     - If conflict in candidate_pool and conflict not in closure:
       - Add to closure
       - Add to queue

4. Return closure
```

**Purpose**: Find all nodes transitively conflicting with `node` within a given candidate pool.

**Example**:
```
A conflicts with B
B conflicts with C
C does not conflict with A

get_conflict_closure(A, {A, B, C}) = {A, B, C}  // Transitive closure
```

### 4.4 Conflict Resolution

#### 4.4.1 Resolve Conflict Set

```cpp
private:
  std::optional<state_delta_ptr> resolve_conflict_set(
    const std::unordered_set<state_delta_ptr>& conflict_set,
    const std::unordered_set<state_delta_ptr>& current_edge_roots,
    const std::optional<protocol::account>& validator_account
  ) const;
```

**Inputs**:
- `conflict_set`: Nodes that mutually conflict
- `current_edge_roots`: Roots currently in the edge set (for tier 4)
- `validator_account`: Optional validator for tier 2

**Output**:
- `Some(winner)`: Replace edge set with winner's descendants
- `None`: Keep current edge set unchanged (tier 4 FIFO)

**Algorithm**:
```
1. If conflict_set has only 1 node:
   - Return Some(that node)

2. Tier 1: Final nodes win
   - Filter conflict_set to final nodes
   - If any final nodes exist:
     - If exactly 1 final node, return Some(that node)
     - Else recurse with filtered set (and same parameters)

3. Tier 2: Validator-approved nodes win (if validator_account provided)
   - For each node, check node->has_approval_from(validator_account)
   - Filter to approved nodes
   - If any approved nodes exist:
     - If exactly 1 approved node, return Some(that node)
     - Else recurse with filtered set (but no validator to avoid infinite loop)

4. Tier 3: Highest approval weight wins
   - Find max = max(node->total_approval() for node in conflict_set)
   - Filter to nodes where total_approval() == max
   - If exactly 1 node, return Some(that node)
   - Else fall through to tier 4

5. Tier 4: FIFO (keep current edge set)
   - For each node in conflict_set:
     - If node in current_edge_roots:
       - Return Some(node)
   - Return None (no node in conflict_set is in current edge set)
```

**Example walkthrough**:

```
Scenario:
  conflict_set = {A, B, C}
  A: total_approval=50, not final, validator approved
  B: total_approval=50, not final, not validator approved
  C: total_approval=30, not final, not validator approved
  validator_account = Some(validator1)

Step 1: conflict_set has 3 nodes, continue

Step 2 (Tier 1): No final nodes, continue

Step 3 (Tier 2): Filter to validator-approved
  filtered = {A}
  Return Some(A)
```

### 4.5 Main Query Functions

#### 4.5.1 Get Final Edges (Simple)

```cpp
public:
  std::vector<state_delta_ptr> get_final_edges() const;
```

**Algorithm**:
```
1. If database not open, throw error

2. Query by_is_final_edge index:
   - Get all nodes where is_final_edge() == true

3. Return as vector
```

**Complexity**: O(f log n) where f is number of final edges, n is total nodes.

**Example**:
```
root (final)
├── child1 (final)
│   └── grandchild1 (not final)
└── child2 (final)

get_final_edges() = [child1, child2]
// root is not included because it has final children
```

#### 4.5.2 Get Edge Candidates (Complex)

```cpp
public:
  std::vector<state_delta_ptr> get_edge_candidates(
    const std::optional<protocol::account>& validator_account = std::nullopt
  ) const;
```

**Algorithm**:
```
1. If database not open, throw error

2. Query by_is_edge_candidate index:
   - candidates = all nodes where is_edge_candidate() == true

3. Build ancestor map for each candidate:
   - candidate_ancestors = map<candidate, set<ancestors>>
   - For each candidate:
     - candidate_ancestors[candidate] = get_all_ancestors(candidate)

4. Find conflict root for each candidate:
   - conflict_root_to_candidates = map<root, set<candidates>>

   - For each candidate:
     - conflict_root = find_highest_conflicting_ancestor(candidate)
     - If conflict_root exists:
       - conflict_root_to_candidates[conflict_root].add(candidate)
     - Else:
       - conflict_root_to_candidates[candidate].add(candidate)

   - find_highest_conflicting_ancestor(candidate):
     - Iterate ancestors from root toward candidate
     - Return first ancestor with non-empty conflicts in _conflict_cache
     - Or nullptr if none found

5. Group conflict roots by transitive conflicts:
   - visited_roots = {}
   - conflict_groups = []

   - For each root in conflict_root_to_candidates.keys():
     - If root in visited_roots:
       - Continue

     - group = get_conflict_closure(root, conflict_root_to_candidates.keys())
     - visited_roots.union(group)
     - conflict_groups.add(group)

6. Resolve each conflict group and collect results:
   - result = []

   - For each group in conflict_groups:
     - If group.size() == 1:
       - winner = group[0]
     - Else:
       - current_edge_roots = conflict_root_to_candidates.keys() ∩ group
       - maybe_winner = resolve_conflict_set(group, current_edge_roots, validator_account)

       - If maybe_winner.has_value():
         - winner = maybe_winner.value()
       - Else:
         - // Tier 4: Keep all current edges (no change)
         - For root in current_edge_roots:
           - result.add_all(conflict_root_to_candidates[root])
         - Continue

     - // Add all edge candidates descended from winner
     - result.add_all(conflict_root_to_candidates[winner])

7. Return result
```

**Complexity**: O(e × a + c²) where:
- e = number of edge candidates
- a = average number of ancestors per candidate
- c = number of conflict roots

**Example walkthrough**:

```
DAG structure:
root
├── A (approval=40)
│   ├── A1 (edge candidate)
│   └── A2 (edge candidate)
└── B (approval=60, conflicts with A)
    ├── B1 (edge candidate)
    └── B2 (edge candidate)

Step 2: candidates = [A1, A2, B1, B2]

Step 3:
  candidate_ancestors[A1] = {A, root}
  candidate_ancestors[A2] = {A, root}
  candidate_ancestors[B1] = {B, root}
  candidate_ancestors[B2] = {B, root}

Step 4:
  find_highest_conflicting_ancestor(A1):
    - Check root: no conflicts
    - Check A: has conflict with B
    - Return A

  conflict_root_to_candidates = {
    A: [A1, A2],
    B: [B1, B2]
  }

Step 5:
  get_conflict_closure(A, {A, B}) = {A, B}  // A conflicts with B
  conflict_groups = [{A, B}]

Step 6:
  group = {A, B}
  current_edge_roots = {A, B}

  resolve_conflict_set({A, B}, {A, B}, None):
    Tier 1: No final nodes
    Tier 2: No validator
    Tier 3: max approval = 60
      Filter to {B}
      Return Some(B)

  winner = B
  result.add_all(conflict_root_to_candidates[B])
  result = [B1, B2]

Step 7: Return [B1, B2]
```

### 4.6 Additional state_delta Method

Add to `state_delta.hpp`:

```cpp
public:
  // Check if a specific account has approved this node
  bool has_approval_from(const protocol::account& approver) const;
```

Add to `state_delta.cpp`:

```cpp
bool state_delta::has_approval_from(const protocol::account& approver) const
{
  return _approvals.contains(approver);
}
```

**Purpose**: Enable tier 2 conflict resolution (validator-approved nodes win).

## 5. Complete Examples

### 5.1 Example 1: Simple Linear Chain

```
Scenario: Linear chain with no conflicts

DAG:
root (complete)
└── block1 (complete)
    └── block2 (complete)
        └── block3 (complete)

Query: get_edge_candidates()

Step 2: candidates = [block3]  // Only childless complete node
Step 4: No conflicts found
Step 5: conflict_groups = [{block3}]
Step 6: winner = block3, result = [block3]

Result: [block3]
```

### 5.2 Example 2: Fork with Conflict Resolution (Tier 3)

```
Scenario: Two conflicting forks, tier 3 resolves

DAG:
root
├── A (approval=75, conflicts with B)
│   └── A1 (edge candidate)
└── B (approval=50, conflicts with A)
    └── B1 (edge candidate)

Query: get_edge_candidates()

Step 2: candidates = [A1, B1]
Step 4: conflict_root_to_candidates = {A: [A1], B: [B1]}
Step 5: conflict_groups = [{A, B}]
Step 6:
  resolve_conflict_set({A, B}, {A, B}, None):
    Tier 3: max approval = 75, filter to {A}
    Return Some(A)
  winner = A
  result = [A1]

Result: [A1]
```

### 5.3 Example 3: Validator-Approved Chain (Tier 2)

```
Scenario: Validator already approved one fork

DAG:
root
├── A (approval=50, validator1 approved, conflicts with B)
│   └── A1 (edge candidate)
└── B (approval=75, not validator1 approved, conflicts with A)
    └── B1 (edge candidate)

Query: get_edge_candidates(validator1)

Step 6:
  resolve_conflict_set({A, B}, {A, B}, Some(validator1)):
    Tier 2: Filter to validator-approved = {A}
    Return Some(A)
  winner = A
  result = [A1]

Result: [A1]
// Even though B has higher approval, validator1 sticks with their chain
```

### 5.4 Example 4: Finalized Node (Tier 1)

```
Scenario: One fork finalized, other not

DAG:
root (final)
├── A (final, conflicts with B)
│   └── A1 (edge candidate)
└── B (not final, conflicts with A)
    └── B1 (edge candidate)

Query: get_edge_candidates()

Step 6:
  resolve_conflict_set({A, B}, {A, B}, None):
    Tier 1: Filter to final = {A}
    Return Some(A)
  winner = A
  result = [A1]

Result: [A1]
// B is excluded because A is finalized
```

### 5.5 Example 5: FIFO Tiebreaker (Tier 4)

```
Scenario: Equal approval, no validator preference

DAG:
root
├── A (approval=50, FIRST SEEN, conflicts with B)
│   └── A1 (edge candidate)
└── B (approval=50, seen later, conflicts with A)
    └── B1 (edge candidate)

Initial state: Edge set was [A1] (A seen first)

Query: get_edge_candidates()

Step 6:
  resolve_conflict_set({A, B}, {A}, None):  // Note: current_edge_roots = {A}
    Tier 1: No final nodes
    Tier 2: No validator
    Tier 3: Both have approval=50, filter to {A, B}
    Tier 4: A is in current_edge_roots
    Return Some(A)
  winner = A
  result = [A1]

Result: [A1]
// Keep first-seen chain (implicit FIFO)
```

### 5.6 Example 6: Three-Way Conflict

```
Scenario: Three conflicting chains

DAG:
root
├── A (approval=60, conflicts with B and C)
│   └── A1 (edge candidate)
├── B (approval=50, conflicts with A and C)
│   └── B1 (edge candidate)
└── C (approval=40, conflicts with A and B)
    └── C1 (edge candidate)

Query: get_edge_candidates()

Step 5:
  get_conflict_closure(A, {A, B, C}) = {A, B, C}
  conflict_groups = [{A, B, C}]

Step 6:
  resolve_conflict_set({A, B, C}, {A, B, C}, None):
    Tier 3: max approval = 60, filter to {A}
    Return Some(A)
  winner = A
  result = [A1]

Result: [A1]
```

### 5.7 Example 7: Diamond DAG (No Conflicts)

```
Scenario: Diamond merge with no conflicts

DAG:
root
├── left (writes key X)
│   └── left_child (edge candidate)
└── right (writes key Y, no conflict with left)
    └── right_child (edge candidate)

Query: get_edge_candidates()

Step 2: candidates = [left_child, right_child]
Step 4:
  No conflicts found in ancestors
  conflict_root_to_candidates = {
    left_child: [left_child],
    right_child: [right_child]
  }
Step 5: conflict_groups = [{left_child}, {right_child}]
Step 6:
  Group {left_child}: winner = left_child, result.add(left_child)
  Group {right_child}: winner = right_child, result.add(right_child)

Result: [left_child, right_child]
// Both included because no conflicts
```

### 5.8 Example 8: Multi-Level Conflict Subtree

```
Scenario: Conflict affects entire subtrees

DAG:
root
├── A (approval=40)
│   ├── A1 (approval=60)
│   │   ├── A1a (edge candidate)
│   │   └── A1b (edge candidate)
│   └── A2 (approval=50)
│       └── A2a (edge candidate)
└── B (approval=80, conflicts with A)
    └── B1 (edge candidate)

Query: get_edge_candidates()

Step 4:
  find_highest_conflicting_ancestor(A1a) = A  // A conflicts with B
  conflict_root_to_candidates = {
    A: [A1a, A1b, A2a],
    B: [B1]
  }

Step 6:
  resolve_conflict_set({A, B}, {A, B}, None):
    Tier 3: max approval = 80, filter to {B}
    Return Some(B)
  winner = B
  result = [B1]

Result: [B1]
// Even though A1 has high approval (60), it's descended from A which lost to B
```

### 5.9 Example 9: Final Edges Query

```
Scenario: Multiple finalization levels

DAG:
root (final)
├── child1 (final)
│   ├── grandchild1 (final)
│   │   └── great1 (not final)
│   └── grandchild2 (not final)
└── child2 (final)
    └── grandchild3 (not final)

Query: get_final_edges()

Analysis:
- root: is_final_edge() = false (has final children: child1, child2)
- child1: is_final_edge() = false (has final child: grandchild1)
- child2: is_final_edge() = true (final, no final children)
- grandchild1: is_final_edge() = true (final, no final children)
- grandchild2: is_final_edge() = false (not final)
- grandchild3: is_final_edge() = false (not final)
- great1: is_final_edge() = false (not final)

Result: [child2, grandchild1]
// These represent epoch boundaries
```

## 6. Implementation Plan

### 6.1 Files to Modify

1. **`include/respublica/state_db/state_delta.hpp`**
   - Add `has_approval_from()` declaration

2. **`src/respublica/state_db/state_delta.cpp`**
   - Implement `has_approval_from()`

3. **`src/respublica/state_db/delta_index.hpp`**
   - Add conflict cache data structures
   - Add public functions: `get_edge_candidates()`, `get_final_edges()`, `cleanup_conflict_cache()`
   - Add private helper functions (8 total)

4. **`src/respublica/state_db/delta_index.cpp`**
   - Modify `mark_complete()` to call `update_conflict_cache_for_node()`
   - Implement all helper functions
   - Implement main query functions

### 6.2 Implementation Order

Recommended order to minimize debugging complexity:

1. **Add `has_approval_from()` to `state_delta`** (trivial, no dependencies)
2. **Add conflict cache data structures to `delta_index`**
3. **Implement `lock_and_filter_conflicts()`** (simple helper)
4. **Implement `get_all_ancestors()`** (simple graph traversal)
5. **Implement `cache_conflict_if_exists()`** (uses `has_conflict()`)
6. **Implement `update_conflict_cache_for_node()`** (uses #5)
7. **Hook `update_conflict_cache_for_node()` into `mark_complete()`**
8. **Implement `cleanup_conflict_cache()`** (uses #3)
9. **Implement `get_conflict_closure()`** (uses #3)
10. **Implement `resolve_conflict_set()`** (uses `has_approval_from()`)
11. **Implement `get_final_edges()`** (simple, good for testing infrastructure)
12. **Implement `get_edge_candidates()`** (complex, uses #4, #9, #10)
13. **Add comprehensive tests**

### 6.3 Testing Strategy

#### Unit Tests (per component)

1. **Conflict caching**:
   - `test_cache_bidirectional_conflicts` - Verify A conflicts B implies B conflicts A in cache
   - `test_cache_weak_ptr_expiration` - Verify expired nodes don't appear in cache
   - `test_cache_incremental_update` - Verify only new node checked on `mark_complete()`
   - `test_cleanup_removes_expired` - Verify `cleanup_conflict_cache()` removes expired pointers
   - `test_cleanup_removes_final` - Verify finalized nodes removed from cache

2. **Helper functions**:
   - `test_get_all_ancestors_linear` - Linear chain
   - `test_get_all_ancestors_diamond` - Diamond DAG
   - `test_get_conflict_closure_transitive` - A→B→C transitive conflicts
   - `test_lock_and_filter_expired` - Verify expired pointers filtered out

3. **Conflict resolution tiers**:
   - `test_tier1_final_wins` - Final node beats non-final
   - `test_tier2_validator_approved_wins` - Validator-approved beats higher approval
   - `test_tier3_highest_approval_wins` - Higher approval wins
   - `test_tier4_fifo_keep_current` - First-seen kept when tied
   - `test_tier_combinations` - Multiple tiers in sequence

4. **Edge candidate queries**:
   - `test_simple_linear_chain` - Example 5.1
   - `test_fork_with_tier3_resolution` - Example 5.2
   - `test_validator_approved_tier2` - Example 5.3
   - `test_finalized_tier1` - Example 5.4
   - `test_fifo_tiebreaker` - Example 5.5
   - `test_three_way_conflict` - Example 5.6
   - `test_diamond_no_conflicts` - Example 5.7
   - `test_multilevel_conflict_subtree` - Example 5.8

5. **Final edge queries**:
   - `test_final_edges_basic` - Example 5.9
   - `test_final_edges_no_final_children` - Single final edge
   - `test_final_edges_empty` - No finalized nodes

#### Integration Tests

1. **Dynamic conflict resolution**:
   - Create conflicting forks with equal approval
   - Add approval to one fork, verify edge set changes
   - Add finalization, verify edge set updates

2. **Validator consistency**:
   - Validator approves fork A
   - Fork B gains more approval
   - Verify validator still builds on fork A (tier 2 override)

3. **Conflict cache lifecycle**:
   - Create conflicts
   - Mark nodes complete (incremental updates)
   - Call cleanup (lazy removal)
   - Deallocate nodes, verify weak_ptr cleanup
   - Verify queries still work correctly

4. **Large DAG performance**:
   - Generate DAG with 100+ nodes and multiple conflicts
   - Measure `get_edge_candidates()` performance
   - Verify cache effectiveness (should not recalculate conflicts)

## 7. Performance Characteristics

### 7.1 Time Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `mark_complete()` (conflict cache update) | O(n) | Check new node against all n nodes |
| `cleanup_conflict_cache()` | O(c) | Iterate c cache entries |
| `get_final_edges()` | O(f log n) | Query index for f final edges |
| `get_edge_candidates()` (worst case) | O(e × a + c²) | e edges, a ancestors each, c conflict roots |
| `get_edge_candidates()` (typical case) | O(e × a) | Few conflicts, O(1) resolution |

### 7.2 Space Complexity

| Structure | Complexity | Notes |
|-----------|------------|-------|
| `_conflict_cache` | O(n × d) | n nodes, average d conflicts each |
| Ancestor maps (temporary) | O(e × a) | Built during query, deallocated after |

### 7.3 Expected Performance

For a typical DAG with:
- 100 active nodes
- 5-10 edge candidates
- 1-2 conflict groups
- Average ancestor depth of 10

**`get_edge_candidates()` breakdown**:
- Step 2 (query index): O(1) via multi-index
- Step 3 (ancestors): 10 nodes × 10 ancestors = 100 lookups
- Step 4-5 (conflict grouping): ~20 conflict checks
- Step 6 (resolution): O(1) for small conflict groups
- **Total**: ~100-200 operations, sub-millisecond

**`mark_complete()` overhead**:
- Conflict cache update: 100 `has_conflict()` calls
- Each `has_conflict()`: O(ancestors) graph traversal
- **Total**: ~1000-10000 operations, potentially 1-10ms

This is acceptable since `mark_complete()` is called once per block validation, not on every query.

### 7.4 Optimization Opportunities (Future Work)

1. **Conflict cache warm-up**: Call `rebuild_conflict_cache()` on startup
2. **Incremental cleanup**: Remove finalized nodes from cache immediately on finalization
3. **Conflict hint tracking**: Store "conflicting ancestors" on each node to speed up `find_highest_conflicting_ancestor()`
4. **Parallel conflict checking**: Use thread pool for `update_conflict_cache_for_node()`

## 8. Edge Cases and Error Handling

### 8.1 Empty DAG

```cpp
// Empty index
get_edge_candidates() → []
get_final_edges() → []
```

### 8.2 All Nodes Conflicting

```
root
├── A (conflicts with B, C, D)
├── B (conflicts with A, C, D)
├── C (conflicts with A, B, D)
└── D (conflicts with A, B, C)

get_edge_candidates() → [winner]  // Single winner from tier 3 or 4
```

### 8.3 Cycle Detection

DAG structure prevents cycles, but if somehow introduced:
- `get_all_ancestors()` uses visited set to prevent infinite loops
- `get_conflict_closure()` uses visited set to prevent infinite loops

### 8.4 Database Not Open

```cpp
if (!is_open())
  throw std::runtime_error("database is not open");
```

All public functions check `is_open()` before proceeding.

### 8.5 Null Validator Account

```cpp
get_edge_candidates(std::nullopt)  // Tier 2 skipped, tiers 1/3/4 apply
```

Tier 2 only applies when `validator_account.has_value()`.

### 8.6 Weak Pointer Expiration During Query

All queries use `lock_and_filter_conflicts()` to safely handle expired pointers. If a node expires mid-query, it's simply excluded from the conflict set.

## 9. Future Enhancements

### 9.1 Conflict Hints

Add to `state_delta`:
```cpp
mutable std::optional<std::weak_ptr<state_delta>> _highest_conflicting_ancestor;
```

Updated during `mark_complete()` to cache the highest conflicting ancestor, speeding up step 4 of `get_edge_candidates()`.

### 9.2 Approval Weight Index

Add to multi-index:
```cpp
boost::multi_index::ordered_non_unique<
  boost::multi_index::tag<by_approval_weight_desc>,
  boost::multi_index::const_mem_fun<state_delta, approval_weight_t, &state_delta::total_approval>,
  std::greater<approval_weight_t>>
```

Enables O(log n) queries for highest-approval nodes within a conflict set.

### 9.3 Conflict Metrics

Track and expose:
- Number of conflicts detected
- Number of conflict resolutions by tier
- Cache hit rate
- Average conflict group size

Useful for monitoring consensus health.

### 9.4 Configurable Conflict Resolution

Allow custom conflict resolution policies:
```cpp
using conflict_resolver = std::function<state_delta_ptr(
  const std::unordered_set<state_delta_ptr>&,
  const std::optional<protocol::account>&)>;

std::vector<state_delta_ptr> get_edge_candidates(
  const std::optional<protocol::account>& validator_account,
  const conflict_resolver& resolver = default_resolver) const;
```

Enables experimentation with different consensus rules.

## 10. Summary

This design provides efficient, conflict-aware edge set queries for DAG-based consensus:

**Key Features**:
- ✅ O(n) incremental conflict cache updates on node completion
- ✅ O(1) weak pointer cleanup (lazy, off critical path)
- ✅ 4-tier conflict resolution (final > validator > approval > FIFO)
- ✅ Subtree-aware conflict handling
- ✅ Memory-efficient weak pointer caching

**Benefits**:
- Validators can quickly identify safe block proposal targets
- Conflict resolution is deterministic and consensus-aligned
- Epoch boundaries (final edges) efficiently queryable
- No manual cache invalidation required
- Testable, maintainable design with clear separation of concerns

**Trade-offs**:
- O(n) overhead per `mark_complete()` for conflict checking
- O(n × d) memory for conflict cache (acceptable for typical DAG sizes)
- Tier 4 implicit FIFO requires tracking "current edge set" state

This design lays the foundation for efficient DAG consensus while maintaining flexibility for future optimizations.
