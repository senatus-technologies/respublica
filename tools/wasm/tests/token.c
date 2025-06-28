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
extern int32_t koinos_check_authority( const char* account_ptr, uint32_t account_len, bool* value );

bool check_authority( account_t acc )
{
  bool authorized = false;

  int32_t code = koinos_check_authority( acc, ACCOUNT_LENGTH, &authorized );

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

enum instructions
{
  authorize_instr,
  name_instr,
  symbol_instr,
  decimals_instr,
  total_supply_instr,
  balance_of_instr,
  transfer_instr,
  mint_instr,
  burn_instr
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
  uint64_t supply;
  uint32_t num_bytes = sizeof( uint64_t );
  int32_t code       = koinos_get_object( supply_id, supply_key, supply_key_size, (char*)&supply, &num_bytes );
  if( code )
    exit( code );

  if( !num_bytes )
    return 0;

  if( num_bytes != sizeof( uint64_t ) )
    exit( errc_invalid_object );

  return supply;
}

uint64_t balance_of( account_t account )
{
  uint64_t balance;
  uint32_t num_bytes = sizeof( uint64_t );

  int32_t code = koinos_get_object( balance_id, account, ACCOUNT_LENGTH, (char*)&balance, &num_bytes );

  if( !num_bytes )
    return 0;

  if( num_bytes != sizeof( uint64_t ) )
    exit( errc_invalid_object );

  return balance;
}

int main( void )
{
  uint32_t entry_point;
  read( STDIN_FILENO, &entry_point, sizeof( uint32_t ) );

  switch( entry_point )
  {
    case authorize_instr:
      {
        const bool no = false;
        write( STDOUT_FILENO, &no, sizeof( bool ) );
        break;
      }
    case name_instr:
      {
        write( STDOUT_FILENO, token_name, strlen( token_name ) );
        break;
      }
    case symbol_instr:
      {
        write( STDOUT_FILENO, token_symbol, strlen( token_symbol ) );
        break;
      }
    case decimals_instr:
      {
        write( STDOUT_FILENO, &token_decimals, sizeof( token_decimals ) );
        break;
      }
    case total_supply_instr:
      {
        uint64_t supply = total_supply();
        write( STDOUT_FILENO, &supply, sizeof( uint64_t ) );
        break;
      }
    case balance_of_instr:
      {
        account_t account;

        read( STDIN_FILENO, account, ACCOUNT_LENGTH );

        uint64_t balance = balance_of( account );
        write( STDOUT_FILENO, &balance, sizeof( uint64_t ) );
        break;
      }
    case transfer_instr:
      {
        account_t from;
        account_t to;
        uint64_t value;

        read( STDIN_FILENO, from, ACCOUNT_LENGTH );
        read( STDIN_FILENO, to, ACCOUNT_LENGTH );
        read( STDIN_FILENO, &value, sizeof( uint64_t ) );

        if( !memcmp( from, to, ACCOUNT_LENGTH ) )
          exit( errc_invalid_argument );

        account_t caller;
        uint32_t num_bytes = ACCOUNT_LENGTH;
        int32_t code       = koinos_get_caller( (char*)caller, &num_bytes );
        if( code )
          exit( code );

        if( num_bytes )
        {
          if( num_bytes != ACCOUNT_LENGTH )
            exit( errc_invalid_object );

          if( memcmp( caller, from, ACCOUNT_LENGTH ) && !check_authority( from ) )
            exit( errc_unauthorized );
        }
        else
        {
          if( !check_authority( from ) )
            exit( errc_unauthorized );
        }

        uint64_t from_balance = balance_of( from );

        if( from_balance < value )
          exit( errc_insufficient_balance );

        uint64_t to_balance = balance_of( to );

        from_balance -= value;
        to_balance   += value;

        koinos_put_object( balance_id, from, ACCOUNT_LENGTH, (char*)&from_balance, sizeof( from_balance ) );
        koinos_put_object( balance_id, to, ACCOUNT_LENGTH, (char*)&to_balance, sizeof( to_balance ) );

        break;
      }
    case mint_instr:
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

        koinos_put_object( supply_id, supply_key, supply_key_size, (char*)&supply, sizeof( supply ) );
        koinos_put_object( balance_id, to, ACCOUNT_LENGTH, (char*)&to_balance, sizeof( to_balance ) );
        break;
      }
    case burn_instr:
      {
        account_t from;
        uint64_t value;

        read( STDIN_FILENO, from, ACCOUNT_LENGTH );
        read( STDIN_FILENO, &value, sizeof( uint64_t ) );

        account_t caller;
        uint32_t num_bytes = ACCOUNT_LENGTH;
        int32_t code       = koinos_get_caller( (char*)caller, &num_bytes );
        if( code )
          exit( code );

        if( num_bytes )
        {
          if( num_bytes != ACCOUNT_LENGTH )
            exit( errc_invalid_object );

          if( memcmp( caller, from, ACCOUNT_LENGTH ) && !check_authority( from ) )
            exit( errc_unauthorized );
        }
        else
        {
          if( !check_authority( from ) )
            exit( errc_unauthorized );
        }

        uint64_t from_balance = balance_of( from );
        if( value > from_balance )
          exit( errc_insufficient_balance );

        uint64_t supply = total_supply();

        if( value > supply )
          exit( errc_insufficient_supply );

        supply       -= value;
        from_balance -= value;

        koinos_put_object( supply_id, supply_key, supply_key_size, (char*)&supply, sizeof( supply ) );
        koinos_put_object( balance_id, from, ACCOUNT_LENGTH, (char*)&from_balance, sizeof( from_balance ) );
        break;
      }
    default:
      {
        exit( errc_invalid_argument );
      }
  }

  return errc_ok;
}
