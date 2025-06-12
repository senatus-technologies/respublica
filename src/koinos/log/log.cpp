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

using namespace std::string_literals;

constexpr auto trace_string   = "trace"s;
constexpr auto debug_string   = "debug"s;
constexpr auto info_string    = "info"s;
constexpr auto warning_string = "warn"s;
constexpr auto error_string   = "error"s;
constexpr auto fatal_string   = "fatal"s;

constexpr auto timestamp_attribute = "TimeStamp"s;
constexpr auto file_attribute      = "File"s;
constexpr auto line_attribute      = "Line"s;
constexpr auto severity_attribute  = "Severity"s;
constexpr auto message_attribute   = "Message"s;

constexpr auto month_width     = 2;
constexpr auto day_width       = 2;
constexpr auto hours_width     = 2;
constexpr auto minutes_width   = 2;
constexpr auto seconds_width   = 2;
constexpr auto useconds_width  = 6;
constexpr auto colored_width   = 14;
constexpr auto uncolored_width = 5;

constexpr auto one_megabyte          = 1 << 20;
constexpr auto one_hundred_megabytes = 100 << 20;
constexpr auto one_hundred           = 100;

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
    auto line  = rec.attribute_values()[ line_attribute ].extract< int >();
    auto file  = rec.attribute_values()[ file_attribute ].extract< std::string >();
    auto ptime = rec.attribute_values()[ timestamp_attribute ].extract< boost::posix_time::ptime >().get();
    auto& s    = std::clog;

    if constexpr( DateTime )
    {
      auto time = ptime.time_of_day();
      auto date = ptime.date();

      s << date.year() << "-";
      s << std::right << std::setfill( '0' ) << std::setw( month_width ) << date.month().as_number() << "-";
      s << std::right << std::setfill( '0' ) << std::setw( day_width ) << date.day() << " ";
      s << std::right << std::setfill( '0' ) << std::setw( hours_width ) << boost::date_time::absolute_value( time.hours() )
        << ":";
      s << std::right << std::setfill( '0' ) << std::setw( minutes_width ) << boost::date_time::absolute_value( time.minutes() )
        << ":";
      s << std::right << std::setfill( '0' ) << std::setw( seconds_width ) << boost::date_time::absolute_value( time.seconds() )
        << ".";
      s << std::right << std::setfill( '0' ) << std::setw( useconds_width )
        << boost::date_time::absolute_value( time.fractional_seconds() );
      s << " ";
    }

    std::string lvl;
    switch( level.get() )
    {
      case boost::log::trivial::severity_level::trace:
        lvl += colorize( trace_string, color::blue );
        break;
      case boost::log::trivial::severity_level::debug:
        lvl += colorize( debug_string, color::blue );
        break;
      case boost::log::trivial::severity_level::info:
        lvl += colorize( info_string, color::green );
        break;
      case boost::log::trivial::severity_level::warning:
        lvl += colorize( warning_string, color::yellow );
        break;
      case boost::log::trivial::severity_level::error:
        lvl += colorize( error_string, color::red );
        break;
      case boost::log::trivial::severity_level::fatal:
        lvl += colorize( fatal_string, color::red );
        break;
      default:
        lvl += colorize( "unknown", color::red );
        break;
    }

    if constexpr( Color )
      s << std::left << std::setfill( ' ' ) << std::setw( colored_width ) << lvl;
    else
      s << std::left << std::setfill( ' ' ) << std::setw( uncolored_width ) << lvl;

    s << " [" << file << ":" << line << "] " << formatted_string << '\n';
  }
};

boost::log::trivial::severity_level level_from_string( const std::string& token )
{
  boost::log::trivial::severity_level l{};

  if( token == trace_string )
  {
    l = boost::log::trivial::severity_level::trace;
  }
  else if( token == debug_string )
  {
    l = boost::log::trivial::severity_level::debug;
  }
  else if( token == info_string )
  {
    l = boost::log::trivial::severity_level::info;
  }
  else if( token == warning_string )
  {
    l = boost::log::trivial::severity_level::warning;
  }
  else if( token == error_string )
  {
    l = boost::log::trivial::severity_level::error;
  }
  else if( token == fatal_string )
  {
    l = boost::log::trivial::severity_level::fatal;
  }
  else
  {
    throw std::runtime_error( "invalid log level" );
  }

  return l;
}

void initialize( std::string_view application_name,
                 std::string_view filter_level,
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

  boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >( severity_attribute );

  if( log_directory.has_value() )
  {
    // Output message to file, rotates when file reached 1mb. Each log file
    // is capped at 1mb and total is 100mb and 100 files.
    boost::log::add_file_log( boost::log::keywords::file_name =
                                log_directory->string() + "/" + std::string( application_name ) + ".log",
                              boost::log::keywords::target_file_name =
                                log_directory->string() + "/" + std::string( application_name ) + "-%Y-%m-%dT%H-%M-%S.%f.log",
                              boost::log::keywords::target        = log_directory->string(),
                              boost::log::keywords::rotation_size = one_megabyte,
                              boost::log::keywords::max_size      = one_hundred_megabytes,
                              boost::log::keywords::max_files     = one_hundred,
                              boost::log::keywords::format = "%" + timestamp_attribute + "% %" + severity_attribute + "% [%" + file_attribute +
                                                             "%:%" + line_attribute + "%]: %" + message_attribute + "%",
                              boost::log::keywords::auto_flush = true );
  }

  boost::log::add_common_attributes();

  boost::log::core::get()->set_filter( boost::log::trivial::severity >= level_from_string( std::string( filter_level ) ) );
}

} // namespace koinos::log
