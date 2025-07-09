#pragma once

#include <quill/LogMacros.h>

#include <koinos/log/formatter.hpp>
#include <koinos/log/frontend.hpp>

namespace koinos::log {

void initialize() noexcept;
logger* instance() noexcept;

} // namespace koinos::log
