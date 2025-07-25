#pragma once

#include <cstdint>

#include <quill/Frontend.h>
#include <quill/Logger.h>

#include <respublica/encode.hpp>

namespace respublica::log {

struct frontend_options
{
  static constexpr quill::QueueType queue_type                    = quill::QueueType::UnboundedDropping;
  static constexpr std::size_t initial_queue_capacity             = 65'536;
  static constexpr std::uint32_t blocking_queue_retry_interval_ns = 800;
  static constexpr std::size_t unbounded_queue_max_capacity       = 2ull * 1'024u * 1'024u * 1'024u;
  static constexpr quill::HugePagesPolicy huge_pages_policy       = quill::HugePagesPolicy::Never;
};

using frontend = quill::FrontendImpl< frontend_options >;
using logger   = quill::LoggerImpl< frontend_options >;

} // namespace respublica::log
