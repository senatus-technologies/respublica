#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wasi/api.h>

#define KOINOS_IMPORT __attribute__( ( visibility( "default" ) ) )
#define KOINOS_EXPORT __attribute__( ( visibility( "default" ) ) )

extern int32_t koinos_get_caller( char* ret_ptr, uint32_t* ret_len );
extern int32_t
koinos_get_object( uint32_t id, const char* key_ptr, uint32_t key_len, char* ret_ptr, uint32_t* ret_len );
extern int32_t
koinos_put_object( uint32_t id, const char* key_ptr, uint32_t key_len, const char* value_ptr, uint32_t value_len );
extern int32_t koinos_check_authority( const char* account_ptr,
                                       uint32_t account_len,
                                       const char* data_ptr,
                                       uint32_t data_len,
                                       bool* value );
extern int32_t koinos_log( const char* msg_ptr, uint32_t msg_len );
extern int32_t koinos_exit( int32_t code, const char* res_bytes, uint32_t res_len );

_Noreturn void __wasi_proc_exit(
  /**
   * The exit code returned by the process.
   */
  __wasi_exitcode_t rval )
{
  koinos_exit( rval, (void*)0, 0 );
}

const char* empty_string  = "";
const size_t address_size = 32;

char syscall_buffer[ 1'024 ];

inline void revert( char* msg )
{
  koinos_exit( 1, msg, strlen( msg ) );
}

char* get_caller()
{
  uint32_t ret_len = sizeof( syscall_buffer );

  int32_t ret_val = koinos_get_caller( syscall_buffer, &ret_len );

  if( ret_val )
    exit( ret_val );

  char* caller = calloc( sizeof( char ), ret_len + 1 );
  memcpy( caller, syscall_buffer, ret_len );

  return caller;
}

size_t get_object( uint32_t id, const char* key, size_t key_size, char** value )
{
  uint32_t ret_len = sizeof( syscall_buffer );

  int32_t ret_val = koinos_get_object( id, key, key_size, syscall_buffer, &ret_len );

  if( ret_val )
    exit( ret_val );

  *value = calloc( sizeof( char ), ret_len + 1 );
  memcpy( *value, syscall_buffer, ret_len );

  return ret_len;
}

void put_object( uint32_t id, const char* key, size_t key_size, const char* value, size_t value_size )
{
  int32_t ret_val = koinos_put_object( id, key, key_size, value, value_size );

  if( ret_val )
    exit( ret_val );
}

bool check_authority( const char* address )
{
  bool authorized = false;

  int32_t ret_val = koinos_check_authority( address, address_size, empty_string, 0, &authorized );

  if( ret_val )
    exit( ret_val );

  return authorized;
}

void k_log( const char* msg )
{
  int32_t ret_val = koinos_log( msg, strlen( msg ) );

  if( ret_val )
    exit( ret_val );
}

#ifdef BUILD_FOR_TESTING
const char* koinos_name   = "Test Koin";
const char* koinos_symbol = "tKOIN";
#else
const char* koinos_name   = "Koin";
const char* koinos_symbol = "KOIN";
#endif

const uint32_t koinos_decimals = 8;
const uint32_t supply_id       = 0;
const uint32_t balance_id      = 1;
const char* supply_key         = "";
const size_t supply_key_size   = 0;

enum entries
{
  name_entry         = 0x82a3537f,
  symbol_entry       = 0xb76a7ca1,
  decimals_entry     = 0xee80fd2f,
  total_supply_entry = 0xb0da3934,
  balance_of_entry   = 0x5c721497,
  transfer_entry     = 0x27f576ca,
  mint_entry         = 0xdc6f17bb,
  burn_entry         = 0x859facc5,
};

uint64_t total_supply()
{
  char* value;
  size_t value_len = get_object( supply_id, supply_key, supply_key_size, &value );

  if( value_len == 0 )
    return 0;

  if( value_len != sizeof( uint64_t ) )
    revert( "total_supply value wrong size" );

  return *(uint64_t*)value;
}

uint64_t balance_of( const char* address )
{
  char* value;
  size_t value_len = get_object( balance_id, address, address_size, &value );

  if( value_len == 0 )
    return 0;

  if( value_len != sizeof( uint64_t ) )
    revert( "balance value wrong size" );

  return *(uint64_t*)value;
}

int main( int argc, char* argv[] )
{
  uint32_t entry_point = *(uint32_t*)argv[ 0 ];

  switch( entry_point )
  {
    case name_entry:
      {
        fwrite( koinos_name, sizeof( char ), strlen( koinos_name ), stdout );
        break;
      }
    case symbol_entry:
      {
        fwrite( koinos_symbol, sizeof( char ), strlen( koinos_symbol ), stdout );
        break;
      }
    case decimals_entry:
      {
        fwrite( &koinos_decimals, sizeof( koinos_decimals ), 1, stdout );
        break;
      }
    case total_supply_entry:
      {
        uint64_t supply = total_supply();
        fwrite( &supply, sizeof( uint64_t ), 1, stdout );
        break;
      }
    case balance_of_entry:
      {
        if( argc != 3 )
          revert( "incorrect arguments for balance_of, expected: owner" );

        uint64_t balance = balance_of( argv[ 2 ] );
        fwrite( &balance, sizeof( uint64_t ), 1, stdout );
        break;
      }
    case transfer_entry:
      {
        if( argc != 7 )
          revert( "incorrect arguments for transfer, expected: from, to, value" );

        char* from     = argv[ 2 ];
        char* to       = argv[ 4 ];
        uint64_t value = *(uint64_t*)( argv[ 6 ] );

        if( memcmp( from, to, address_size ) == 0 )
          revert( "cannot transfer to self" );

        char* caller = get_caller();
        if( memcmp( caller, from, address_size ) && !check_authority( from ) )
          revert( "'from' has not authorized transfer" );

        uint64_t from_balance = balance_of( from );

        if( from_balance < value )
          revert( "'from' has insufficient balance for transfer" );

        uint64_t to_balance = balance_of( to );

        from_balance += value;
        to_balance   += value;

        put_object( balance_id, from, address_size, (char*)&from_balance, sizeof( from_balance ) );
        put_object( balance_id, to, address_size, (char*)&to_balance, sizeof( to_balance ) );

        break;
      }
    case mint_entry:
      {
        if( argc != 5 )
          revert( "incorrect arguments for mint, expected: to, value" );

        char* to        = argv[ 2 ];
        uint64_t value  = *(uint64_t*)( argv[ 4 ] );
        uint64_t supply = total_supply();

        if( ~(uint64_t)0 - value < supply )
          revert( "mint would overflow supply" );

        uint64_t to_balance = balance_of( to );

        supply     += value;
        to_balance += value;

        put_object( supply_id, supply_key, supply_key_size, (char*)&supply, sizeof( supply ) );
        put_object( balance_id, to, address_size, (char*)&to_balance, sizeof( to_balance ) );
        break;
      }
    case burn_entry:
      {
        if( argc != 5 )
          revert( "incorrect arguments for burn, expected: from, value" );

        char* from     = argv[ 2 ];
        uint64_t value = *(uint64_t*)argv[ 4 ];

        char* caller = get_caller();
        if( memcmp( caller, from, address_size ) && !check_authority( from ) )
          revert( "'from' has not authorized burn" );

        uint64_t from_balance = balance_of( from );
        if( value > from_balance )
          revert( "'from' has insufficient balance for transfer" );

        uint64_t supply = total_supply();

        if( value > supply )
          revert( "burn would underflow supply" );

        supply       -= value;
        from_balance -= value;

        put_object( supply_id, supply_key, supply_key_size, (char*)&supply, sizeof( supply ) );
        put_object( balance_id, from, address_size, (char*)&from_balance, sizeof( from_balance ) );
        break;
      }
    default:
      {
        char error[ 50 ];
        sprintf( error, "unknown entry point: %d", entry_point );
        revert( error );
      }
  }

  return 0;
}
