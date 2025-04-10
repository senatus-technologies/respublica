#pragma once

#include <cstddef>
#include <string>

#include <koinos/chain/chain.pb.h>
#include <koinos/crypto/multihash.hpp>

namespace koinos::state_db {

using state_node_id = crypto::multihash;
using object_space  = chain::object_space;
using object_key    = std::string;
using object_value  = std::string;

} // namespace koinos::state_db
