#include <respublica/controller/session.hpp>

namespace respublica::controller {

session::session( std::uint64_t initial_resources ):
    resource_session( initial_resources ),
    frame_recorder_session()
{}

} // namespace respublica::controller
