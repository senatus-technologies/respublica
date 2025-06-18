#include <algorithm>
#include <boost/endian.hpp>
#include <koinos/controller/coin.hpp>
#include <koinos/log.hpp>
#include <koinos/memory.hpp>
#include <koinos/protocol.hpp>

namespace koinos::controller {

result< std::uint64_t > coin::total_supply( system_interface* system )
{
  auto object = system->get_object( supply_id, std::span< const std::byte >{} );
  if( !object.size() )
    return 0;

  if( object.size() != sizeof( std::uint64_t ) )
    return std::unexpected( reversion_errc::failure );

  auto supply = memory::bit_cast< std::uint64_t >( object );
  boost::endian::little_to_native_inplace( supply );
  return supply;
}

result< std::uint64_t > coin::balance_of( system_interface* system, std::span< const std::byte > address )
{
  auto object = system->get_object( balance_id, address );
  if( !object.size() )
    return 0;

  if( object.size() != sizeof( std::uint64_t ) )
    return std::unexpected( reversion_errc::failure );

  auto balance = memory::bit_cast< std::uint64_t >( object );
  boost::endian::little_to_native_inplace( balance );
  return balance;
}

std::error_code coin::start( system_interface* system, const std::span< const std::span< const std::byte > > args )
{
  auto entry_point = memory::bit_cast< std::uint32_t >( args[ 0 ] );
  boost::endian::little_to_native_inplace( entry_point );

  switch( entry_point )
  {
    case name_entry:
      {
        system->write_output( memory::as_bytes( name ) );
        break;
      }
    case symbol_entry:
      {
        system->write_output( memory::as_bytes( symbol ) );
        break;
      }
    case decimals_entry:
      {
        auto dec = boost::endian::native_to_little( decimals );
        system->write_output( memory::as_bytes( dec ) );
        break;
      }
    case total_supply_entry:
      {
        auto supply = total_supply( system );
        if( !supply.has_value() )
          return supply.error();

        boost::endian::native_to_little_inplace( *supply );
        system->write_output( memory::as_bytes( *supply ) );
        break;
      }
    case balance_of_entry:
      {
        if( args.size() != 2 )
          return reversion_errc::failure;

        auto balance = balance_of( system, args[ 1 ] );
        if( !balance.has_value() )
          return balance.error();

        boost::endian::native_to_little_inplace( *balance );
        system->write_output( memory::as_bytes( *balance ) );
        break;
      }
    case transfer_entry:
      {
        if( args.size() != 4 )
          return reversion_errc::failure;

        auto from  = args[ 1 ];
        auto to    = args[ 2 ];
        auto value = memory::bit_cast< std::uint64_t >( args[ 3 ] );
        boost::endian::little_to_native_inplace( value );

        if( std::ranges::equal( from, to ) )
          return reversion_errc::failure;

        auto caller = system->get_caller();

        koinos::protocol::account from_acct;
        assert( from_acct.size() == from.size() );
        std::memcpy( from_acct.data(), from.data(), from.size() );
        if( !std::ranges::equal( from, caller ) && !system->check_authority( from_acct ) )
          return reversion_errc::failure;

        auto from_balance = balance_of( system, from );
        if( !from_balance.has_value() )
          return reversion_errc::failure;

        if( *from_balance < value )
          return reversion_errc::failure;

        auto to_balance = balance_of( system, to );
        if( !to_balance.has_value() )
          return reversion_errc::failure;

        *from_balance -= value;
        *to_balance   += value;

        boost::endian::native_to_little_inplace( *from_balance );
        boost::endian::native_to_little_inplace( *to_balance );

        system->put_object( balance_id, from, memory::as_bytes( *from_balance ) );
        system->put_object( balance_id, to, memory::as_bytes( *to_balance ) );

        break;
      }
    case mint_entry:
      {
        if( args.size() != 3 )
          return reversion_errc::failure;

        auto to    = args[ 1 ];
        auto value = memory::bit_cast< std::uint64_t >( args[ 2 ] );
        boost::endian::little_to_native_inplace( value );

        auto supply = total_supply( system );
        if( !supply.has_value() )
          return reversion_errc::failure;

        if( ~(std::uint64_t)0 - value < *supply )
          return reversion_errc::failure;

        auto to_balance = balance_of( system, to );
        if( !to_balance.has_value() )
          return reversion_errc::failure;

        *supply     += value;
        *to_balance += value;

        boost::endian::native_to_little_inplace( *supply );
        boost::endian::native_to_little_inplace( *to_balance );

        system->put_object( supply_id, std::span< const std::byte >{}, memory::as_bytes( *supply ) );
        system->put_object( balance_id, to, memory::as_bytes( *to_balance ) );
        break;
      }
    case burn_entry:
      {
        if( args.size() != 3 )
          return reversion_errc::failure;

        auto from  = args[ 1 ];
        auto value = memory::bit_cast< std::uint64_t >( args[ 2 ] );
        boost::endian::little_to_native_inplace( value );

        auto caller = system->get_caller();

        koinos::protocol::account from_acct;
        assert( from_acct.size() == from.size() );
        std::memcpy( from_acct.data(), from.data(), from.size() );

        if( !std::ranges::equal( from, caller ) && !system->check_authority( from_acct ) )
          return reversion_errc::failure;

        auto from_balance = balance_of( system, from );
        if( !from_balance.has_value() )
          return reversion_errc::failure;

        if( *from_balance < value )
          return reversion_errc::failure;

        auto supply = total_supply( system );
        if( !supply.has_value() )
          return reversion_errc::failure;

        if( value > *supply )
          return reversion_errc::failure;

        *supply       -= value;
        *from_balance -= value;

        boost::endian::native_to_little_inplace( *supply );
        boost::endian::native_to_little_inplace( *from_balance );

        system->put_object( supply_id, std::span< const std::byte >{}, memory::as_bytes( *supply ) );
        system->put_object( balance_id, from, memory::as_bytes( *from_balance ) );
        break;
      }
  }

  return reversion_errc::ok;
}

} // namespace koinos::controller
