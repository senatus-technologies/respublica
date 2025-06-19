#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup.hpp>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define LOG( LEVEL )                                                                                                   \
  BOOST_LOG_SEV( ::boost::log::trivial::logger::get(), boost::log::trivial::LEVEL )                                    \
    << boost::log::add_value( "Line", __LINE__ )                                                                       \
    << boost::log::add_value( "File", boost::filesystem::path( __FILE__ ).filename().string() )

// NOLINTEND(cppcoreguidelines-macro-usage)

namespace koinos::log {

void initialize( std::string_view application_name,
                 std::string_view filter_level,
                 const std::optional< std::filesystem::path >& log_directory = {},
                 bool color                                                  = true,
                 bool datetime                                               = false );

inline void initialize( const std::string& application_name,
                        const std::string& filter_level                             = "info",
                        const std::optional< std::filesystem::path >& log_directory = {},
                        bool color                                                  = true,
                        bool datetime                                               = false )
{
  initialize( std::string_view( application_name ), std::string_view( filter_level ), log_directory, color, datetime );
}

} // namespace koinos::log
