#include <koinos/log/log.hpp>

#include <quill/Backend.h>
#include <quill/DeferredFormatCodec.h>
#include <quill/Frontend.h>
#include <quill/HelperMacros.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/Utility.h>

namespace koinos::log {

void initialize() noexcept
{
  quill::Backend::start();
}

logger* get() noexcept
{
  return frontend::create_or_get_logger(
    "root",
    frontend::create_or_get_sink< quill::ConsoleSink >( "console_sink_id_1" ),
    quill::PatternFormatterOptions{ "%(time) [%(thread_id)] %(short_source_location:<24) %(log_level:<9) "
                                    "%(tags)%(message)",
                                    "%Y-%m-%d %H:%M:%S.%Qms",
                                    quill::Timezone::GmtTime } );
}

} // namespace koinos::log
