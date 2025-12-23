#pragma once

#include <memory>
#include <vector>

#include <quill/LogMacros.h>
#include <quill/sinks/Sink.h>

#include <respublica/log/formatter.hpp>
#include <respublica/log/frontend.hpp>

namespace respublica::log {

void initialize( std::vector< std::shared_ptr< quill::Sink > > sinks = {} ) noexcept;
logger* instance() noexcept;

} // namespace respublica::log
