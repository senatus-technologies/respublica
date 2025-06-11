#include <koinos/util/base64.hpp>

#include <base64.h>

#include <sstream>

namespace koinos::util {

std::string to_base64( const std::string& s, bool websafe )
{
  return base64_encode( s, websafe );
}

std::string from_base64( const std::string& s )
{
  return base64_decode( s );
}

} // namespace koinos::util
