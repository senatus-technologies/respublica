#include <koinos/log/log.hpp>
#include <koinos/util/hex.hpp>
#include <koinos/util/random.hpp>

#include <iostream>
#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/log/attributes/attribute_value.hpp>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/log/sinks/basic_sink_backend.hpp>

#define TRACE_STRING   "trace"
#define DEBUG_STRING   "debug"
#define INFO_STRING    "info"
#define WARNING_STRING "warn"
#define ERROR_STRING   "error"
#define FATAL_STRING   "fatal"

#define TIMESTAMP_ATTR  "TimeStamp"
#define SERVICE_ID_ATTR "ServiceID"
#define FILE_ATTR       "File"
#define LINE_ATTR       "Line"
#define SEVERITY_ATTR   "Severity"
#define MESSAGE_ATTR    "Message"

namespace koinos::log {

template< bool Color, bool DateTime >
class console_sink_impl
    : public boost::log::sinks::basic_formatted_sink_backend< char, boost::log::sinks::synchronized_feeding >
{
  enum class color : uint8_t
  {
    green,
    yellow,
    red,
    blue
  };

  static std::string colorize( const std::string& s, color c )
  {
    if constexpr( !Color )
      return s;

    std::string val = "";

    switch( c )
    {
      case color::green:
        val += "\033[32m";
        break;
      case color::yellow:
        val += "\033[33m";
        break;
      case color::red:
        val += "\033[31m";
        break;
      case color::blue:
        val += "\033[34m";
        break;
    }

    val += s;
    val += "\033[0m";

    return val;
  }

public:
  static void consume( const boost::log::record_view& rec, const string_type& formatted_string )
  {
    auto level = rec[ boost::log::trivial::severity ];
    auto line  = rec.attribute_values()[ LINE_ATTR ].extract< int >();
    auto file  = rec.attribute_values()[ FILE_ATTR ].extract< std::string >();
    auto ptime = rec.attribute_values()[ TIMESTAMP_ATTR ].extract< boost::posix_time::ptime >().get();
    auto& s    = std::clog;

    if constexpr( DateTime )
    {
      auto time = ptime.time_of_day();
      auto date = ptime.date();

      s << date.year() << "-";
      s << std::right << std::setfill( '0' ) << std::setw( 2 ) << date.month().as_number() << "-";
      s << std::right << std::setfill( '0' ) << std::setw( 2 ) << date.day() << " ";
      s << std::right << std::setfill( '0' ) << std::setw( 2 ) << boost::date_time::absolute_value( time.hours() )
        << ":";
      s << std::right << std::setfill( '0' ) << std::setw( 2 ) << boost::date_time::absolute_value( time.minutes() )
        << ":";
      s << std::right << std::setfill( '0' ) << std::setw( 2 ) << boost::date_time::absolute_value( time.seconds() )
        << ".";
      s << std::right << std::setfill( '0' ) << std::setw( 6 )
        << boost::date_time::absolute_value( time.fractional_seconds() );
      s << " ";
    }

    std::string lvl;
    switch( level.get() )
    {
      case boost::log::trivial::severity_level::trace:
        lvl += colorize( TRACE_STRING, color::blue );
        break;
      case boost::log::trivial::severity_level::debug:
        lvl += colorize( DEBUG_STRING, color::blue );
        break;
      case boost::log::trivial::severity_level::info:
        lvl += colorize( INFO_STRING, color::green );
        break;
      case boost::log::trivial::severity_level::warning:
        lvl += colorize( WARNING_STRING, color::yellow );
        break;
      case boost::log::trivial::severity_level::error:
        lvl += colorize( ERROR_STRING, color::red );
        break;
      case boost::log::trivial::severity_level::fatal:
        lvl += colorize( FATAL_STRING, color::red );
        break;
      default:
        lvl += colorize( "unknown", color::red );
        break;
    }

    if constexpr( Color )
      s << std::left << std::setfill( ' ' ) << std::setw( 14 ) << lvl;
    else
      s << std::left << std::setfill( ' ' ) << std::setw( 5 ) << lvl;

    s << " [" << file << ":" << line << "] " << formatted_string << std::endl;
  }
};

boost::log::trivial::severity_level level_from_string( const std::string& token )
{
  boost::log::trivial::severity_level l;

  if( token == TRACE_STRING )
  {
    l = boost::log::trivial::severity_level::trace;
  }
  else if( token == DEBUG_STRING )
  {
    l = boost::log::trivial::severity_level::debug;
  }
  else if( token == INFO_STRING )
  {
    l = boost::log::trivial::severity_level::info;
  }
  else if( token == WARNING_STRING )
  {
    l = boost::log::trivial::severity_level::warning;
  }
  else if( token == ERROR_STRING )
  {
    l = boost::log::trivial::severity_level::error;
  }
  else if( token == FATAL_STRING )
  {
    l = boost::log::trivial::severity_level::fatal;
  }
  else
  {
    throw std::runtime_error( "invalid log level" );
  }

  return l;
}

void initialize( const std::string& application_name,
                 const std::string& filter_level,
                 const std::optional< std::filesystem::path >& log_directory,
                 bool color,
                 bool datetime )
{
  using console_sink                = boost::log::sinks::synchronous_sink< console_sink_impl< false, false > >;
  using console_datetime_sink       = boost::log::sinks::synchronous_sink< console_sink_impl< false, true > >;
  using color_console_sink          = boost::log::sinks::synchronous_sink< console_sink_impl< true, false > >;
  using color_console_datetime_sink = boost::log::sinks::synchronous_sink< console_sink_impl< true, true > >;

  if( color )
    if( datetime )
      boost::log::core::get()->add_sink( boost::make_shared< color_console_datetime_sink >() );
    else
      boost::log::core::get()->add_sink( boost::make_shared< color_console_sink >() );
  else if( datetime )
    boost::log::core::get()->add_sink( boost::make_shared< console_datetime_sink >() );
  else
    boost::log::core::get()->add_sink( boost::make_shared< console_sink >() );

  boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >( SEVERITY_ATTR );

  if( log_directory.has_value() )
  {
    // Output message to file, rotates when file reached 1mb. Each log file
    // is capped at 1mb and total is 100mb and 100 files.
    boost::log::add_file_log( boost::log::keywords::file_name =
                                log_directory->string() + "/" + application_name + ".log",
                              boost::log::keywords::target_file_name =
                                log_directory->string() + "/" + application_name + "-%Y-%m-%dT%H-%M-%S.%f.log",
                              boost::log::keywords::target        = log_directory->string(),
                              boost::log::keywords::rotation_size = 1 * 1'024 * 1'024,
                              boost::log::keywords::max_size      = 100 * 1'024 * 1'024,
                              boost::log::keywords::max_files     = 100,
                              boost::log::keywords::format = "%" TIMESTAMP_ATTR "% %" SEVERITY_ATTR "% [%" FILE_ATTR
                                                             "%:%" LINE_ATTR "%]: %" MESSAGE_ATTR "%",
                              boost::log::keywords::auto_flush = true );
  }

  boost::log::add_common_attributes();

  boost::log::core::get()->set_filter( boost::log::trivial::severity >= level_from_string( filter_level ) );
}

} // namespace koinos::log
