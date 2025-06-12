#pragma once
#include <koinos/chain/types.hpp>
#include <koinos/protocol/protocol.hpp>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace koinos::chain {

using event_bundle = std::pair< std::optional< crypto::digest >, protocol::event >;

struct chronicler_session
{
  void push_event( const protocol::event& ev );
  const std::vector< protocol::event >& events() const;

  void push_log( const std::string& message );
  void push_log( std::string&& message );
  const std::vector< std::string >& logs() const;

private:
  std::vector< protocol::event > _events;
  std::vector< std::string > _logs;
};

class chronicler final
{
public:
  void set_session( const std::shared_ptr< chronicler_session >& s );
  void push_event( std::optional< crypto::digest > transaction_id, protocol::event&& ev );
  void push_log( std::span< const std::byte > message );
  void push_log( std::string_view message );
  void push_log( const std::string& message );
  void push_log( std::string&& message );
  const std::vector< event_bundle >& events();
  const std::vector< std::string >& logs();

private:
  std::weak_ptr< chronicler_session > _session;
  std::vector< event_bundle > _events;
  std::vector< std::string > _logs;
  uint32_t _seq_no = 0;
};

} // namespace koinos::chain
