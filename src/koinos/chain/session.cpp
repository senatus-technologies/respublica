#include <koinos/chain/exceptions.hpp>
#include <koinos/chain/session.hpp>

namespace koinos::chain {

session::session( uint64_t begin_rc ): rc_session( begin_rc ), chronicler_session()
{}

} // namespace koinos::chain
