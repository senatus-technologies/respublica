#pragma once

#include <quill/LogMacros.h>

#include <respublica/log/formatter.hpp>
#include <respublica/log/frontend.hpp>

namespace respublica::log {

void initialize() noexcept;
logger* instance() noexcept;

} // namespace respublica::log
