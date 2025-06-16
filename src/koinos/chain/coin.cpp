#include <algorithm>
#include <boost/endian.hpp>
#include <koinos/chain/coin.hpp>
#include <koinos/log/log.hpp>
#include <koinos/memory/memory.hpp>
#include <koinos/protocol/protocol.hpp>

namespace koinos::chain {

result< std::uint64_t > coin::total_supply( system_interface* system )
{
  auto object = system->get_object( supply_id, std::span< const std::byte >{} );
  if( !object.has_value() || !object->size() )
    return 0;

  if( object->size() != sizeof( uint64_t ) )
    return std::unexpected( reversion_errc::failure );

  return boost::endian::little_to_native( *memory::start_lifetime_as< const uint64_t >( object->data() ) );
}

result< std::uint64_t > coin::balance_of( system_interface* system, std::span< const std::byte > address )
{
  auto object = system->get_object( balance_id, address );
  if( !object.has_value() || !object->size() )
    return 0;

  if( object->size() != sizeof( uint64_t ) )
    return std::unexpected( reversion_errc::failure );

  return boost::endian::little_to_native( *memory::start_lifetime_as< const uint64_t >( object->data() ) );
}

std::error_code coin::start( system_interface* system, const std::span< const std::span< const std::byte > > args )
{
  std::uint32_t entry_point =
    boost::endian::little_to_native( *memory::start_lifetime_as< const std::uint32_t >( args[ 0 ].data() ) );
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
        if( args.size() != 2 )
          return reversion_errc::failure;

        auto balance = balance_of( system, args[ 1 ] );
        if( !balance.has_value() )
          return balance.error();

        boost::endian::native_to_little_inplace( *balance );
        system->write_output( std::as_bytes( std::span< uint64_t >( &*balance, 1 ) ) );
        break;
      }
    case transfer_entry:
      {
        if( args.size() != 4 )
          return reversion_errc::failure;

        auto from = args[ 1 ];
        auto to   = args[ 2 ];
        uint64_t value =
          boost::endian::little_to_native( *memory::start_lifetime_as< const uint64_t >( args[ 3 ].data() ) );

        if( std::ranges::equal( from, to ) )
          return reversion_errc::failure;

        auto caller = system->get_caller();
        if( !caller.has_value() )
          return reversion_errc::failure;

        koinos::protocol::account from_acct;
        assert( from_acct.size() == from.size() );
        std::memcpy( from_acct.data(), from.data(), from.size() );
        if( !std::ranges::equal( from, *caller ) && !system->check_authority( from_acct ) )
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

        system->put_object( balance_id, from, std::as_bytes( std::span< uint64_t >( &*from_balance, 1 ) ) );
        system->put_object( balance_id, to, std::as_bytes( std::span< uint64_t >( &*to_balance, 1 ) ) );

        break;
      }
    case mint_entry:
      {
        if( args.size() != 3 )
          return reversion_errc::failure;

        auto to = args[ 1 ];
        uint64_t value =
          boost::endian::little_to_native( *memory::start_lifetime_as< const uint64_t >( args[ 2 ].data() ) );

        auto supply = total_supply( system );
        if( !supply.has_value() )
          return reversion_errc::failure;

        if( ~(uint64_t)0 - value < *supply )
          return reversion_errc::failure;

        auto to_balance = balance_of( system, to );
        if( !to_balance.has_value() )
          return reversion_errc::failure;

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
        if( args.size() != 3 )
          return reversion_errc::failure;

        auto from = args[ 1 ];
        uint64_t value =
          boost::endian::little_to_native( *memory::start_lifetime_as< const uint64_t >( args[ 2 ].data() ) );

        auto caller = system->get_caller();
        if( !caller.has_value() )
          return reversion_errc::failure;

        koinos::protocol::account from_acct;
        assert( from_acct.size() == from.size() );
        std::memcpy( from_acct.data(), from.data(), from.size() );

        if( !std::ranges::equal( from, *caller ) && !system->check_authority( from_acct ) )
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

        system->put_object( supply_id,
                            std::span< const std::byte >{},
                            std::as_bytes( std::span< uint64_t >( &*supply, 1 ) ) );
        system->put_object( balance_id, from, std::as_bytes( std::span< uint64_t >( &*from_balance, 1 ) ) );
        break;
      }
  }

  return reversion_errc::ok;
}

} // namespace koinos::chain
