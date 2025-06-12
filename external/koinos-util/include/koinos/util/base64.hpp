#pragma once

#include <string>

namespace koinos::util {

template< typename T >
std::string to_base64( const T&, bool websafe = true );

template< typename T >
T from_base64( const std::string& );

template<>
std::string to_base64< std::string >( const std::string&, bool websafe );

template<>
std::string from_base64< std::string >( const std::string& s );

} // namespace koinos::util
