#include <koinos/controller/session.hpp>

namespace koinos::controller {

session::session( std::uint64_t initial_resources ):
    resource_session( initial_resources ),
    chronicler_session()
{}

} // namespace koinos::controller
