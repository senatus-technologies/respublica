#pragma once

#include <span>

#include <quill/BinaryDataDeferredFormatCodec.h>
#include <quill/Frontend.h>
#include <quill/Logger.h>
#include <quill/LogMacros.h>

#include <koinos/encode.hpp>

namespace koinos::log {

struct frontend_options
{
  static constexpr quill::QueueType queue_type                    = quill::QueueType::BoundedDropping;
  static constexpr std::size_t initial_queue_capacity             = 131'072;
  static constexpr std::uint32_t blocking_queue_retry_interval_ns = 800;
  static constexpr std::size_t unbounded_queue_max_capacity       = 2ull * 1'024u * 1'024u * 1'024u;
  static constexpr quill::HugePagesPolicy huge_pages_policy       = quill::HugePagesPolicy::Never;
};

using frontend = quill::FrontendImpl< frontend_options >;
using logger   = quill::LoggerImpl< frontend_options >;

struct encode_hex
{};

using hex = quill::BinaryData< encode_hex >;

struct encode_base58
{};

using base58 = quill::BinaryData< encode_base58 >;

void initialize() noexcept;
logger* get() noexcept;

} // namespace koinos::log

template<>
struct fmtquill::formatter< koinos::log::hex >
{
  constexpr auto parse( format_parse_context& ctx )
  {
    return ctx.begin();
  }

  auto format( const koinos::log::hex& bin_data, format_context& ctx ) const
  {
    return fmtquill::format_to( ctx.out(),
                                "{}",
                                koinos::encode::to_hex( std::span( bin_data.data(), bin_data.size() ) ) );
  }
};

template<>
struct quill::Codec< koinos::log::hex >: quill::BinaryDataDeferredFormatCodec< koinos::log::hex >
{};

template<>
struct fmtquill::formatter< koinos::log::base58 >
{
  constexpr auto parse( format_parse_context& ctx )
  {
    return ctx.begin();
  }

  auto format( const koinos::log::base58& bin_data, format_context& ctx ) const
  {
    return fmtquill::format_to( ctx.out(),
                                "{}",
                                koinos::encode::to_base58( std::span( bin_data.data(), bin_data.size() ) ) );
  }
};

template<>
struct quill::Codec< koinos::log::base58 >: quill::BinaryDataDeferredFormatCodec< koinos::log::base58 >
{};
