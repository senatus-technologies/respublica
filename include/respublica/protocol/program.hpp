#pragma once

#include <string>
#include <vector>

#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>

#include <respublica/protocol/account.hpp>

namespace respublica::protocol {

struct program_input
{
  program_input() noexcept                                  = default;
  program_input( const program_input& ) noexcept            = default;
  program_input( program_input&& ) noexcept                 = default;
  program_input& operator=( const program_input& ) noexcept = default;
  program_input& operator=( program_input&& ) noexcept      = default;
  virtual ~program_input() noexcept                         = default;

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

struct program_frame final: program_input,
                            program_output
{
  program_frame() noexcept                                  = default;
  program_frame( const program_frame& ) noexcept            = default;
  program_frame( program_frame&& ) noexcept                 = default;
  program_frame& operator=( const program_frame& ) noexcept = default;
  program_frame& operator=( program_frame&& ) noexcept      = default;
  ~program_frame() noexcept final                           = default;

  account id{};
  std::uint32_t depth = 0;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int version )
  {
    ar& boost::serialization::base_object< program_input >( *this );
    ar& boost::serialization::base_object< program_output >( *this );
    ar & id;
    ar & depth;
  }
};

} // namespace respublica::protocol
