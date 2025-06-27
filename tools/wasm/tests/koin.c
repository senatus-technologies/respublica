#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ACCOUNT_LENGTH 33
typedef char account_t[ ACCOUNT_LENGTH ];

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

const char* empty_string = "";

char syscall_buffer[ 1'024 ];

char* get_caller()
{
  uint32_t ret_len = sizeof( syscall_buffer );

  int32_t code = koinos_get_caller( syscall_buffer, &ret_len );

  if( code )
    exit( code );

  char* caller = calloc( sizeof( char ), ret_len + 1 );
  memcpy( caller, syscall_buffer, ret_len );

  return caller;
}

size_t get_object( uint32_t id, const char* key, size_t key_size, char** value )
{
  uint32_t ret_len = sizeof( syscall_buffer );

  int32_t code = koinos_get_object( id, key, key_size, syscall_buffer, &ret_len );

  if( code )
    exit( code );

  *value = calloc( sizeof( char ), ret_len + 1 );
  memcpy( *value, syscall_buffer, ret_len );

  return ret_len;
}

void put_object( uint32_t id, const char* key, size_t key_size, const char* value, size_t value_size )
{
  int32_t code = koinos_put_object( id, key, key_size, value, value_size );

  if( code )
    exit( code );
}

bool check_authority( account_t acc )
{
  bool authorized = false;

  int32_t code = koinos_check_authority( acc, ACCOUNT_LENGTH, empty_string, 0, &authorized );

  if( code )
    exit( code );

  return authorized;
}

const char* token_name   = "Token";
const char* token_symbol = "TOKEN";

const uint32_t token_decimals = 8;
const uint32_t supply_id      = 0;
const uint32_t balance_id     = 1;
const char* supply_key        = "";
const size_t supply_key_size  = 0;

enum entry_points
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

enum errc
{
  errc_ok                   = 0,
  errc_unauthorized         = 1,
  errc_insufficient_balance = 2,
  errc_insufficient_supply  = 3,
  errc_invalid_argument     = 4,
  errc_invalid_object       = 5,
  errc_overflow             = 6
};

uint64_t total_supply()
{
  char* value;
  size_t value_len = get_object( supply_id, supply_key, supply_key_size, &value );

  if( value_len == 0 )
    return 0;

  if( value_len != sizeof( uint64_t ) )
  {
    exit( errc_invalid_object );
  }

  return *(uint64_t*)value;
}

uint64_t balance_of( account_t account )
{
  char* value;
  size_t value_len = get_object( balance_id, account, ACCOUNT_LENGTH, &value );

  if( value_len == 0 )
    return 0;

  if( value_len != sizeof( uint64_t ) )
  {
    exit( errc_invalid_object );
  }

  return *(uint64_t*)value;
}

int main( void )
{
  uint32_t entry_point;
  read( STDIN_FILENO, &entry_point, sizeof( uint32_t ) );

  switch( entry_point )
  {
    case name_entry:
      {
        write( STDOUT_FILENO, token_name, strlen( token_name ) );
        break;
      }
    case symbol_entry:
      {
        write( STDOUT_FILENO, token_symbol, strlen( token_symbol ) );
        break;
      }
    case decimals_entry:
      {
        write( STDOUT_FILENO, &token_decimals, sizeof( token_decimals ) );
        break;
      }
    case total_supply_entry:
      {
        uint64_t supply = total_supply();
        write( STDOUT_FILENO, &supply, sizeof( uint64_t ) );
        break;
      }
    case balance_of_entry:
      {
        account_t account;

        read( STDIN_FILENO, account, ACCOUNT_LENGTH );

        uint64_t balance = balance_of( account );
        write( STDOUT_FILENO, &balance, sizeof( uint64_t ) );
        break;
      }
    case transfer_entry:
      {
        account_t from;
        account_t to;
        uint64_t value;

        read( STDIN_FILENO, from, ACCOUNT_LENGTH );
        read( STDIN_FILENO, to, ACCOUNT_LENGTH );
        read( STDIN_FILENO, &value, sizeof( uint64_t ) );

        if( memcmp( from, to, ACCOUNT_LENGTH ) == 0 )
          exit( errc_invalid_argument );

        char* caller = get_caller();
        if( memcmp( caller, from, ACCOUNT_LENGTH ) && !check_authority( from ) )
          exit( errc_unauthorized );

        uint64_t from_balance = balance_of( from );

        if( from_balance < value )
          exit( errc_insufficient_balance );

        uint64_t to_balance = balance_of( to );

        from_balance -= value;
        to_balance   += value;

        put_object( balance_id, from, ACCOUNT_LENGTH, (char*)&from_balance, sizeof( from_balance ) );
        put_object( balance_id, to, ACCOUNT_LENGTH, (char*)&to_balance, sizeof( to_balance ) );

        break;
      }
    case mint_entry:
      {
        account_t to;
        uint64_t value;

        read( STDIN_FILENO, to, ACCOUNT_LENGTH );
        read( STDIN_FILENO, &value, sizeof( uint64_t ) );

        uint64_t supply = total_supply();

        if( UINT64_MAX - value < supply )
          exit( errc_overflow );

        uint64_t to_balance = balance_of( to );

        supply     += value;
        to_balance += value;

        put_object( supply_id, supply_key, supply_key_size, (char*)&supply, sizeof( supply ) );
        put_object( balance_id, to, ACCOUNT_LENGTH, (char*)&to_balance, sizeof( to_balance ) );
        break;
      }
    case burn_entry:
      {
        account_t from;
        uint64_t value;

        read( STDIN_FILENO, from, ACCOUNT_LENGTH );
        read( STDIN_FILENO, &value, sizeof( uint64_t ) );

        char* caller = get_caller();
        if( memcmp( caller, from, ACCOUNT_LENGTH ) && !check_authority( from ) )
          exit( errc_unauthorized );

        uint64_t from_balance = balance_of( from );
        if( value > from_balance )
          exit( errc_insufficient_balance );

        uint64_t supply = total_supply();

        if( value > supply )
          exit( errc_insufficient_supply );

        supply       -= value;
        from_balance -= value;

        put_object( supply_id, supply_key, supply_key_size, (char*)&supply, sizeof( supply ) );
        put_object( balance_id, from, ACCOUNT_LENGTH, (char*)&from_balance, sizeof( from_balance ) );
        break;
      }
    default:
      {
        exit( errc_invalid_argument );
      }
  }

  return errc_ok;
}
