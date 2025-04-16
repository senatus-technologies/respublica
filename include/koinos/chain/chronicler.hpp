#pragma once
#include <koinos/chain/types.hpp>
#include <koinos/protocol/protocol.pb.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace koinos::chain {

using event_bundle = std::pair< std::optional< std::string >, protocol::event_data >;

struct chronicler_session
{
  void push_event( const protocol::event_data& ev );
  const std::vector< protocol::event_data >& events() const;

  void push_log( const std::string& message );
  void push_log( std::string&& message );
  const std::vector< std::string >& logs() const;

private:
  std::vector< protocol::event_data > _events;
  std::vector< std::string > _logs;
};

class chronicler final
{
public:
  void set_session( std::shared_ptr< chronicler_session > s );
  void push_event( std::optional< std::string > transaction_id, protocol::event_data&& ev );
  void push_log( bytes_s message );
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
