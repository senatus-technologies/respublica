#pragma once

#include <chrono>
#include <cstdint>
#include <span>

#include <quill/BinaryDataDeferredFormatCodec.h>
#include <quill/DeferredFormatCodec.h>
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

void initialize() noexcept;
logger* get() noexcept;

struct encode_hex
{};

using hex = quill::BinaryData< encode_hex >;

struct encode_base58
{};

using base58 = quill::BinaryData< encode_base58 >;

struct time_remaining
{
  std::chrono::system_clock::time_point now;
  std::chrono::milliseconds timestamp;
};

struct percent
{
  std::uint64_t numerator;
  std::uint64_t denominator;
};

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

template<>
struct fmtquill::formatter< koinos::log::time_remaining >
{
  constexpr auto parse( format_parse_context& ctx ) -> decltype( ctx.begin() )
  {
    return ctx.begin();
  }

  auto format( const koinos::log::time_remaining& tr, format_context& ctx ) const
  {
    auto time = std::chrono::duration_cast< std::chrono::seconds >( tr.now.time_since_epoch()
                                                                    - std::chrono::milliseconds( tr.timestamp ) )
                  .count();

    std::stringstream ss;
    constexpr auto seconds_per_minute = 60;
    constexpr auto minutes_per_hour   = 60;
    constexpr auto hours_per_day      = 24;
    constexpr auto days_per_year      = 365;

    auto seconds  = time % seconds_per_minute;
    time         /= seconds_per_minute;
    auto minutes  = time % minutes_per_hour;
    time         /= minutes_per_hour;
    auto hours    = time % hours_per_day;
    time         /= hours_per_day;
    auto days     = time % days_per_year;
    auto years    = time / days_per_year;

    if( years )
    {
      ss << years << "y, " << days << "d, ";
    }
    else if( days )
    {
      ss << days << "d, ";
    }

    ss << std::setw( 2 ) << std::setfill( '0' ) << hours;
    ss << std::setw( 1 ) << "h, ";
    ss << std::setw( 2 ) << std::setfill( '0' ) << minutes;
    ss << std::setw( 1 ) << "m, ";
    ss << std::setw( 2 ) << std::setfill( '0' ) << seconds;
    ss << std::setw( 1 ) << "s";

    return fmtquill::format_to( ctx.out(), "{}", ss.str() );
  }
};

template<>
struct quill::Codec< koinos::log::time_remaining >: quill::DeferredFormatCodec< koinos::log::time_remaining >
{};

template<>
struct fmtquill::formatter< koinos::log::percent >
{
  constexpr auto parse( format_parse_context& ctx ) -> decltype( ctx.begin() )
  {
    return ctx.begin();
  }

  auto format( const koinos::log::percent& p, format_context& ctx ) const
  {
    static constexpr auto one_hundred_percent = 100;
    auto percent = static_cast< double >( p.numerator ) / static_cast< double >( p.denominator ) * one_hundred_percent;

    return fmtquill::format_to( ctx.out(), "{}%", percent );
  }
};

template<>
struct quill::Codec< koinos::log::percent >: quill::DeferredFormatCodec< koinos::log::percent >
{};
