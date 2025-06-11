#pragma once

#include <filesystem>
#include <string_view>

namespace koinos::util {

namespace service {
constexpr std::string_view chain = "chain";
} // namespace service

std::filesystem::path get_default_base_directory();

} // namespace koinos::util
