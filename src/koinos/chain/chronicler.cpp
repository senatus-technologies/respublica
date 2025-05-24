#include <koinos/chain/chronicler.hpp>

namespace koinos::chain {

/*
 * Chronicler session
 */

void chronicler_session::push_event( const protocol::event& ev )
{
  _events.push_back( ev );
}

const std::vector< protocol::event >& chronicler_session::events() const
{
  return _events;
}

void chronicler_session::push_log( const std::string& message )
{
  _logs.emplace_back( message );
}

void chronicler_session::push_log( std::string&& message )
{
  _logs.emplace_back( std::move( message ) );
}

const std::vector< std::string >& chronicler_session::logs() const
{
  return _logs;
}

/*
 * Chronicler
 */

void chronicler::set_session( std::shared_ptr< chronicler_session > s )
{
  _session = s;
}

void chronicler::push_event( std::optional< protocol::digest > transaction_id, protocol::event&& ev )
{
  ev.sequence = _seq_no;

  if( auto session = _session.lock() )
    session->push_event( ev );

  _events.emplace_back( std::make_pair( transaction_id, std::move( ev ) ) );
  _seq_no++;
}

void chronicler::push_log( bytes_s message )
{
  if( auto session = _session.lock() )
    session->push_log( std::string( reinterpret_cast< const char* >( message.data() ), message.size() ) );
  else
    _logs.emplace_back( std::string( reinterpret_cast< const char* >( message.data() ), message.size() ) );
}

void chronicler::push_log( std::string_view message )
{
  if( auto session = _session.lock() )
    session->push_log( std::string( message ) );
  else
    _logs.emplace_back( std::string( message ) );
}

void chronicler::push_log( const std::string& message )
{
  if( auto session = _session.lock() )
    session->push_log( message );
  else
    _logs.push_back( message );
}

void chronicler::push_log( std::string&& message )
{
  if( auto session = _session.lock() )
    session->push_log( std::move( message ) );
  else
    _logs.emplace_back( std::move( message ) );
}

const std::vector< event_bundle >& chronicler::events()
{
  return _events;
}

const std::vector< std::string >& chronicler::logs()
{
  return _logs;
}

} // namespace koinos::chain
