#include <koinos/vm_manager/timer.hpp>

#include <koinos/log/log.hpp>

namespace koinos::timer::detail {

using namespace std::chrono_literals;

std::map< std::string, std::chrono::steady_clock::duration > times;

void timer_log()
{
  for( const auto& entry: times )
    LOG( info ) << entry.first << ", " << ( entry.second / 1.0s );
}

timer::~timer()
{
  auto stop       = std::chrono::steady_clock::now();
  times[ label ] += ( stop - start );
}

} // namespace koinos::timer::detail
