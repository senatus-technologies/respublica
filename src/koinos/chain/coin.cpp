#include <algorithm>
#include <boost/endian/conversion.hpp>
#include <koinos/chain/coin.hpp>
#include <koinos/log/log.hpp>
#include <koinos/protocol/protocol.hpp>
#include <koinos/util/base58.hpp>
#include <koinos/util/conversion.hpp>

namespace koinos::chain {

using namespace koinos::error;

std::expected< uint64_t, error > coin::total_supply( system_interface* system )
{
  auto object = system->get_object( supply_id, std::span< const std::byte >{} );
  if( !object.has_value() || !object->size() )
    return 0;

  if( object->size() != sizeof( uint64_t ) )
    return std::unexpected( error_code::reversion );

  return boost::endian::little_to_native( *(uint64_t*)object->data() );
}

std::expected< uint64_t, error > coin::balance_of( system_interface* system, std::span< const std::byte > address )
{
  auto object = system->get_object( balance_id, address );
  if( !object.has_value() || !object->size() )
    return 0;

  if( object->size() != sizeof( uint64_t ) )
    return std::unexpected( error_code::reversion );

  return boost::endian::little_to_native( *(uint64_t*)object->data() );
}

error coin::start( system_interface* system,
                   uint32_t entry_point,
                   const std::vector< std::span< const std::byte > >& args )
{
  switch( entry_point )
  {
    case name_entry:
      {
        system->write_output( std::as_bytes( std::span( name ) ) );
        break;
      }
    case symbol_entry:
      {
        system->write_output( std::as_bytes( std::span( symbol ) ) );
        break;
      }
    case decimals_entry:
      {
        auto dec = boost::endian::native_to_little( decimals );
        system->write_output( std::as_bytes( std::span< uint64_t >( &dec, 1 ) ) );
        break;
      }
    case total_supply_entry:
      {
        auto supply = total_supply( system );
        if( !supply.has_value() )
          return supply.error();

        boost::endian::native_to_little_inplace( *supply );
        system->write_output( std::as_bytes( std::span< uint64_t >( &*supply, 1 ) ) );
        break;
      }
    case balance_of_entry:
      {
        if( args.size() != 1 )
          return error( error_code::reversion );

        auto balance = balance_of( system, args[ 0 ] );
        if( !balance.has_value() )
          return balance.error();

        boost::endian::native_to_little_inplace( *balance );
        system->write_output( std::as_bytes( std::span< uint64_t >( &*balance, 1 ) ) );
        break;
      }
    case transfer_entry:
      {
        if( args.size() != 3 )
          return error( error_code::reversion );

        auto from      = args[ 0 ];
        auto to        = args[ 1 ];
        uint64_t value = boost::endian::little_to_native( *(uint64_t*)args[ 2 ].data() );

        if( std::equal( from.begin(), from.end(), to.begin(), to.end() ) )
          return error( error_code::reversion );

        auto caller = system->get_caller();
        if( !caller.has_value() )
          return error( error_code::reversion );

        koinos::protocol::account from_acct;
        assert( from_acct.size() == from.size() );
        std::memcpy( from_acct.data(), from.data(), from.size() );
        if( !std::equal( from.begin(), from.end(), caller->begin(), caller->end() )
            && !system->check_authority( from_acct ) )
          return error( error_code::reversion );

        auto from_balance = balance_of( system, from );
        if( !from_balance.has_value() )
          return error( error_code::reversion );

        if( *from_balance < value )
          return error( error_code::reversion );

        auto to_balance = balance_of( system, to );
        if( !to_balance.has_value() )
          return error( error_code::reversion );

        *from_balance -= value;
        *to_balance   += value;

        boost::endian::native_to_little_inplace( *from_balance );
        boost::endian::native_to_little_inplace( *to_balance );

        system->put_object( balance_id, from, std::as_bytes( std::span< uint64_t >( &*from_balance, 1 ) ) );
        system->put_object( balance_id, to, std::as_bytes( std::span< uint64_t >( &*to_balance, 1 ) ) );

        break;
      }
    case mint_entry:
      {
        if( args.size() != 2 )
          return error( error_code::reversion );

        auto to        = args[ 0 ];
        uint64_t value = boost::endian::little_to_native( *(uint64_t*)args[ 1 ].data() );

        auto supply = total_supply( system );
        if( !supply.has_value() )
          return error( error_code::reversion );

        if( ~(uint64_t)0 - value < *supply )
          return error( error_code::reversion );

        auto to_balance = balance_of( system, to );
        if( !to_balance.has_value() )
          return error( error_code::reversion );

        *supply     += value;
        *to_balance += value;

        boost::endian::native_to_little_inplace( *supply );
        boost::endian::native_to_little_inplace( *to_balance );

        system->put_object( supply_id,
                            std::span< const std::byte >{},
                            std::as_bytes( std::span< uint64_t >( &*supply, 1 ) ) );
        system->put_object( balance_id, to, std::as_bytes( std::span< uint64_t >( &*to_balance, 1 ) ) );
        break;
      }
    case burn_entry:
      {
        if( args.size() != 2 )
          return error( error_code::reversion );

        auto from      = args[ 0 ];
        uint64_t value = boost::endian::little_to_native( *(uint64_t*)args[ 1 ].data() );

        auto caller = system->get_caller();
        if( !caller.has_value() )
          return error( error_code::reversion );

        koinos::protocol::account from_acct;
        assert( from_acct.size() == from.size() );
        std::memcpy( from_acct.data(), from.data(), from.size() );

        if( !std::equal( from.begin(), from.end(), caller->begin(), caller->end() )
            && !system->check_authority( from_acct ) )
          return error( error_code::reversion );

        auto from_balance = balance_of( system, from );
        if( !from_balance.has_value() )
          return error( error_code::reversion );

        if( *from_balance < value )
          return error( error_code::reversion );

        auto supply = total_supply( system );
        if( !supply.has_value() )
          return error( error_code::reversion );

        if( value > *supply )
          return error( error_code::reversion );

        *supply       -= value;
        *from_balance -= value;

        boost::endian::native_to_little_inplace( *supply );
        boost::endian::native_to_little_inplace( *from_balance );

        system->put_object( supply_id,
                            std::span< const std::byte >{},
                            std::as_bytes( std::span< uint64_t >( &*supply, 1 ) ) );
        system->put_object( balance_id, from, std::as_bytes( std::span< uint64_t >( &*from_balance, 1 ) ) );
        break;
      }
  }

  return error( error_code::success );
}

} // namespace koinos::chain
