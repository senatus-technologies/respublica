#include <algorithm>
#include <boost/endian.hpp>
#include <koinos/log.hpp>
#include <koinos/memory.hpp>
#include <koinos/program/coin.hpp>
#include <koinos/protocol.hpp>
#include <limits>

namespace koinos::program {

result< std::uint64_t > coin::total_supply( program_interface* system )
{
  auto object = system->get_object( supply_id, std::span< const std::byte >{} );
  if( !object.size() )
    return 0;

  if( object.size() != sizeof( std::uint64_t ) )
    return std::unexpected( program_errc::failure );

  auto supply = memory::bit_cast< std::uint64_t >( object );
  boost::endian::little_to_native_inplace( supply );
  return supply;
}

result< std::uint64_t > coin::balance_of( program_interface* system, std::span< const std::byte > account )
{
  auto object = system->get_object( balance_id, account );
  if( !object.size() )
    return 0;

  if( object.size() != sizeof( std::uint64_t ) )
    return std::unexpected( program_errc::failure );

  auto balance = memory::bit_cast< std::uint64_t >( object );
  boost::endian::little_to_native_inplace( balance );
  return balance;
}

std::error_code coin::run( program_interface* system, const std::span< const std::string > arguments )
{
  std::uint32_t instruction = 0;
  system->read( file_descriptor::stdin, memory::as_writable_bytes( instruction ) );
  boost::endian::little_to_native_inplace( instruction );

  switch( instruction )
  {
    case std::to_underlying( instruction::authorize ):
      {
        static constexpr bool no = false;
        system->write( file_descriptor::stdout, memory::as_bytes( no ) );
        break;
      }
    case std::to_underlying( instruction::name ):
      {
        system->write( file_descriptor::stdout, memory::as_bytes( name ) );
        break;
      }
    case std::to_underlying( instruction::symbol ):
      {
        system->write( file_descriptor::stdout, memory::as_bytes( symbol ) );
        break;
      }
    case std::to_underlying( instruction::decimals ):
      {
        auto dec = boost::endian::native_to_little( decimals );
        system->write( file_descriptor::stdout, memory::as_bytes( dec ) );
        break;
      }
    case std::to_underlying( instruction::total_supply ):
      {
        auto supply = total_supply( system );
        if( !supply.has_value() )
          return supply.error();

        boost::endian::native_to_little_inplace( *supply );
        system->write( file_descriptor::stdout, memory::as_bytes( *supply ) );
        break;
      }
    case std::to_underlying( instruction::balance_of ):
      {
        protocol::account account;
        system->read( file_descriptor::stdin, memory::as_writable_bytes( account ) );

        auto balance = balance_of( system, account );
        if( !balance.has_value() )
          return balance.error();

        boost::endian::native_to_little_inplace( *balance );
        system->write( file_descriptor::stdout, memory::as_bytes( *balance ) );
        break;
      }
    case std::to_underlying( instruction::transfer ):
      {
        protocol::account from;
        protocol::account to;
        std::uint64_t value = 0;

        system->read( file_descriptor::stdin, memory::as_writable_bytes( from ) );
        system->read( file_descriptor::stdin, memory::as_writable_bytes( to ) );
        system->read( file_descriptor::stdin, memory::as_writable_bytes( value ) );

        boost::endian::little_to_native_inplace( value );

        if( std::ranges::equal( from, to ) )
          return program_errc::failure;

        auto caller = system->get_caller();

        if( !std::ranges::equal( from, caller ) && !system->check_authority( from ) )
          return program_errc::failure;

        auto from_balance = balance_of( system, from );
        if( !from_balance.has_value() )
          return program_errc::failure;

        if( *from_balance < value )
          return program_errc::failure;

        auto to_balance = balance_of( system, to );
        if( !to_balance.has_value() )
          return program_errc::failure;

        *from_balance -= value;
        *to_balance   += value;

        boost::endian::native_to_little_inplace( *from_balance );
        boost::endian::native_to_little_inplace( *to_balance );

        system->put_object( balance_id, from, memory::as_bytes( *from_balance ) );
        system->put_object( balance_id, to, memory::as_bytes( *to_balance ) );

        break;
      }
    case std::to_underlying( instruction::mint ):
      {
        protocol::account to;
        std::uint64_t value = 0;

        system->read( file_descriptor::stdin, memory::as_writable_bytes( to ) );
        system->read( file_descriptor::stdin, memory::as_writable_bytes( value ) );

        boost::endian::little_to_native_inplace( value );

        auto supply = total_supply( system );
        if( !supply.has_value() )
          return program_errc::failure;

        if( std::numeric_limits< std::uint64_t >::max() - value < *supply )
          return program_errc::failure;

        auto to_balance = balance_of( system, to );
        if( !to_balance.has_value() )
          return program_errc::failure;

        *supply     += value;
        *to_balance += value;

        boost::endian::native_to_little_inplace( *supply );
        boost::endian::native_to_little_inplace( *to_balance );

        system->put_object( supply_id, std::span< const std::byte >{}, memory::as_bytes( *supply ) );
        system->put_object( balance_id, to, memory::as_bytes( *to_balance ) );
        break;
      }
    case std::to_underlying( instruction::burn ):
      {
        protocol::account from;
        std::uint64_t value = 0;

        system->read( file_descriptor::stdin, memory::as_writable_bytes( from ) );
        system->read( file_descriptor::stdin, memory::as_writable_bytes( value ) );

        boost::endian::little_to_native_inplace( value );

        auto caller = system->get_caller();

        if( !std::ranges::equal( from, caller ) && !system->check_authority( from ) )
          return program_errc::failure;

        auto from_balance = balance_of( system, from );
        if( !from_balance.has_value() )
          return program_errc::failure;

        if( *from_balance < value )
          return program_errc::failure;

        auto supply = total_supply( system );
        if( !supply.has_value() )
          return program_errc::failure;

        if( value > *supply )
          return program_errc::failure;

        *supply       -= value;
        *from_balance -= value;

        boost::endian::native_to_little_inplace( *supply );
        boost::endian::native_to_little_inplace( *from_balance );

        system->put_object( supply_id, std::span< const std::byte >{}, memory::as_bytes( *supply ) );
        system->put_object( balance_id, from, memory::as_bytes( *from_balance ) );
        break;
      }
    default:
      return program_errc::failure;
  }

  return program_errc::ok;
}

} // namespace koinos::program
