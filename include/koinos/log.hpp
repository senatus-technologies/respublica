#pragma once

#include <filesystem>
#include <istream>
#include <optional>
#include <ostream>

#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup.hpp>

#include <google/protobuf/message.h>

namespace google::protobuf {
std::ostream& operator<<( std::ostream& os, const google::protobuf::Message& m );
} // namespace google::protobuf

#define LOG( LEVEL )                                                                                                   \
  BOOST_LOG_SEV( ::boost::log::trivial::logger::get(), boost::log::trivial::LEVEL )                                    \
    << boost::log::add_value( "Line", __LINE__ )                                                                       \
    << boost::log::add_value( "File", boost::filesystem::path( __FILE__ ).filename().string() )

namespace koinos {

void initialize_logging( const std::string& application_name,
                         const std::optional< std::string >& identifier              = {},
                         const std::string& filter_level                             = "info",
                         const std::optional< std::filesystem::path >& log_directory = {},
                         bool color                                                  = true,
                         bool datetime                                               = true );

} // namespace koinos
