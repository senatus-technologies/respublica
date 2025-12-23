#include <respublica/log/log.hpp>

#include <chrono>

#include <quill/Backend.h>
#include <quill/backend/BackendOptions.h>
#include <quill/sinks/ConsoleSink.h>

namespace respublica::log {

static std::vector< std::shared_ptr< quill::Sink > >
  configured_sinks; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void initialize( std::vector< std::shared_ptr< quill::Sink > > sinks ) noexcept
{
  configured_sinks = std::move( sinks );

  constexpr auto sleep_duration = std::chrono::seconds{ 1 };

  quill::BackendOptions options;
  options.sleep_duration = sleep_duration;
  options.error_notifier = []( const std::string& err ) noexcept
  {
    LOG_ERROR( respublica::log::instance(), "Encountered backend logging error: {}", err );
  };

  quill::Backend::start( options );
}

logger* instance() noexcept
{
  static auto logger_instance = []() -> logger*
  {
    std::vector< std::shared_ptr< quill::Sink > > sinks;

    if( !configured_sinks.empty() )
    {
      // Use user-provided sinks
      sinks = configured_sinks;
    }
    else
    {
      // Create default console sink
      sinks.push_back( frontend::create_or_get_sink< quill::ConsoleSink >( "console_sink_id_1" ) );
    }

    return frontend::create_or_get_logger(
      "root",
      sinks,
      quill::PatternFormatterOptions{ "%(time) [%(thread_id)] %(short_source_location:<28) %(log_level_short_code:<2) "
                                      "%(tags)%(message)",
                                      "%Y-%m-%d %H:%M:%S.%Qms",
                                      quill::Timezone::GmtTime } );
  }();

  return logger_instance;
}

} // namespace respublica::log
