#pragma once

#include <string>
#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>

#include <koinos/protocol/account.hpp>

namespace koinos::protocol {

struct program_input
{
  std::vector< std::string > arguments;
  std::vector< std::byte > stdin;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & arguments;
    ar & stdin;
  }
};

struct program_output
{
  program_output() noexcept                                   = default;
  program_output( const program_output& ) noexcept            = default;
  program_output( program_output&& ) noexcept                 = default;
  program_output& operator=( const program_output& ) noexcept = default;
  program_output& operator=( program_output&& ) noexcept      = default;
  virtual ~program_output() noexcept                          = default;

  std::int32_t code = 0;
  std::vector< std::byte > stdout;
  std::vector< std::byte > stderr;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar & code;
    ar & stdout;
    ar & stderr;
  }
};

struct program_frame final: program_output
{
  program_frame() noexcept                                  = default;
  program_frame( const program_frame& ) noexcept            = default;
  program_frame( program_frame&& ) noexcept                 = default;
  program_frame& operator=( const program_frame& ) noexcept = default;
  program_frame& operator=( program_frame&& ) noexcept      = default;
  ~program_frame() noexcept final                           = default;

  std::uint32_t depth = 0;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar& boost::serialization::base_object< program_output >( *this );
    ar & depth;
  }
};

} // namespace koinos::protocol
