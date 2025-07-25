#include <koinos/log/log.hpp>

#include <chrono>

#include <quill/Backend.h>
#include <quill/backend/BackendOptions.h>
#include <quill/sinks/ConsoleSink.h>

namespace koinos::log {

void initialize() noexcept
{
  constexpr auto sleep_duration = std::chrono::seconds{ 1 };

  quill::BackendOptions options;
  options.sleep_duration = sleep_duration;
  options.error_notifier = []( const std::string& err ) noexcept
  {
    LOG_ERROR( koinos::log::instance(), "Encountered backend logging error: {}", err );
  };

  quill::Backend::start( options );
}

logger* instance() noexcept
{
  static auto logger = frontend::create_or_get_logger(
    "root",
    frontend::create_or_get_sink< quill::ConsoleSink >( "console_sink_id_1" ),
    quill::PatternFormatterOptions{ "%(time) [%(thread_id)] %(short_source_location:<28) %(log_level_short_code:<2) "
                                    "%(tags)%(message)",
                                    "%Y-%m-%d %H:%M:%S.%Qms",
                                    quill::Timezone::GmtTime } );
  return logger;
}

} // namespace koinos::log
