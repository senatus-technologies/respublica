#pragma once

#include <chrono>
#include <span>
#include <stdexcept>
#include <type_traits>

#include <quill/BinaryDataDeferredFormatCodec.h>
#include <quill/DeferredFormatCodec.h>

#include <respublica/encode.hpp>

namespace respublica::log {

struct hex_tag
{};

using hex = quill::BinaryData< hex_tag >;

struct base58_tag
{};

using base58 = quill::BinaryData< base58_tag >;

struct time_remaining
{
  std::chrono::system_clock::time_point now;
  std::chrono::milliseconds timestamp;
};

template< typename T1, typename T2 >
  requires std::is_integral_v< T1 > && std::is_integral_v< T2 >
struct percent
{
  T1 numerator;
  T2 denominator;
};

} // namespace respublica::log

template<>
struct fmtquill::formatter< respublica::log::hex >
{
  constexpr auto parse( format_parse_context& ctx )
  {
    return ctx.begin();
  }

  auto format( const respublica::log::hex& bin_data, format_context& ctx ) const
  {
    return fmtquill::format_to( ctx.out(),
                                "{}",
                                respublica::encode::to_hex( std::span( bin_data.data(), bin_data.size() ) ) );
  }
};

template<>
struct quill::Codec< respublica::log::hex >: quill::BinaryDataDeferredFormatCodec< respublica::log::hex >
{};

template<>
struct fmtquill::formatter< respublica::log::base58 >
{
  constexpr auto parse( format_parse_context& ctx )
  {
    return ctx.begin();
  }

  auto format( const respublica::log::base58& bin_data, format_context& ctx ) const
  {
    return fmtquill::format_to( ctx.out(),
                                "{}",
                                respublica::encode::to_base58( std::span( bin_data.data(), bin_data.size() ) ) );
  }
};

template<>
struct quill::Codec< respublica::log::base58 >: quill::BinaryDataDeferredFormatCodec< respublica::log::base58 >
{};

template<>
struct fmtquill::formatter< respublica::log::time_remaining >
{
  constexpr auto parse( format_parse_context& ctx )
  {
    return ctx.begin();
  }

  auto format( const respublica::log::time_remaining& tr, format_context& ctx ) const
  {
    auto time = std::chrono::duration_cast< std::chrono::seconds >( tr.now.time_since_epoch() - tr.timestamp ).count();

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

    std::stringstream ss;

    if( years )
      ss << years << "y, " << days << "d, ";
    else if( days )
      ss << days << "d, ";

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
struct quill::Codec< respublica::log::time_remaining >: quill::DeferredFormatCodec< respublica::log::time_remaining >
{};

template< typename T1, typename T2 >
struct fmtquill::formatter< respublica::log::percent< T1, T2 > >
{
  constexpr auto parse( format_parse_context& ctx )
  {
    return ctx.begin();
  }

  auto format( const respublica::log::percent< T1, T2 >& p, format_context& ctx ) const
  {
    static constexpr auto one_hundred_percent = 100;

    if( !p.denominator )
      throw std::runtime_error( "percent formatter divide by zero" );

    auto percent = static_cast< double >( p.numerator ) / static_cast< double >( p.denominator ) * one_hundred_percent;
    return fmtquill::format_to( ctx.out(), "{}%", percent );
  }
};

template< typename T1, typename T2 >
struct quill::Codec< respublica::log::percent< T1, T2 > >: quill::DeferredFormatCodec< respublica::log::percent< T1, T2 > >
{};
