#include <koinos/chain/session.hpp>

namespace koinos::chain {

session::session( std::uint64_t initial_resources ):
    rc_session( initial_resources ),
    chronicler_session()
{}

} // namespace koinos::chain
