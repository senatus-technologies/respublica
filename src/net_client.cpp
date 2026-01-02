#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <print>
#include <sstream>
#include <thread>
#include <vector>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/program_options.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <quill/sinks/Sink.h>

#include <respublica/log.hpp>
#include <respublica/net.hpp>

constexpr unsigned short default_port = 43'333;

struct chat_message
{
  std::string message;

  template< class Archive >
  void serialize( Archive& ar, const unsigned int /*version*/ )
  {
    ar & message;
  }
};

namespace respublica::net {
template<>
struct message_type_traits< chat_message >
{
  static constexpr message_type_id type_id = message_type_id( 1'000 );
};
} // namespace respublica::net

// Log entry with level for color coding
struct log_entry
{
  std::string message;
  quill::LogLevel level;
};

// Application state (thread-safe)
struct app_state
{
  mutable std::mutex mutex;
  std::vector< std::string > messages;
  std::vector< log_entry > logs;

  void add_message( const std::string& msg )
  {
    std::lock_guard lock( mutex );
    messages.push_back( msg );
  }

  void add_log( const std::string& log, quill::LogLevel level )
  {
    std::lock_guard lock( mutex );
    logs.push_back( { log, level } );
  }

  std::vector< std::string > get_messages() const
  {
    std::lock_guard lock( mutex );
    return messages;
  }

  std::vector< log_entry > get_logs() const
  {
    std::lock_guard lock( mutex );
    return logs;
  }
};

// Custom Quill sink that captures logs to app state
class TuiSink: public quill::Sink
{
public:
  explicit TuiSink( app_state& state ):
      _state( state )
  {}

  void write_log( const quill::MacroMetadata* /*log_metadata*/,
                  uint64_t /*log_timestamp*/,
                  std::string_view /*thread_id*/,
                  std::string_view /*thread_name*/,
                  const std::string& /*process_id*/,
                  std::string_view /*logger_name*/,
                  quill::LogLevel log_level,
                  std::string_view /*log_level_description*/,
                  std::string_view /*log_level_short_code*/,
                  const std::vector< std::pair< std::string, std::string > >* /*named_args*/,
                  std::string_view /*log_message*/,
                  std::string_view log_statement ) noexcept override
  {
    // log_statement contains the fully formatted log message
    _state.add_log( std::string( log_statement ), log_level );
  }

  void flush_sink() noexcept override
  {
    // Nothing to flush for in-memory storage
  }

  void run_periodic_tasks() noexcept override
  {
    // No periodic tasks needed
  }

private:
  app_state& _state;
};

auto main( int argc, char** argv ) -> int
{
  boost::program_options::options_description options;

  // clang-format off
  options.add_options()
    ( "help,h"    , "Print this help message and exit" )
    ( "version,v" , "Print version string and exit" )
    ( "port,p"    , boost::program_options::value< std::uint16_t >()->default_value( default_port ), "Default listening port" )
    ( "endpoint,e", boost::program_options::value< std::string >(), "The endpoint to connect to")
    ( "cert"      , boost::program_options::value< std::string >()->default_value( "cert.pem" ), "Path to certificate file" )
    ( "key"       , boost::program_options::value< std::string >()->default_value( "server.pem" ), "Path to private key file" );
  // clang-format on

  boost::program_options::variables_map args;
  boost::program_options::store( boost::program_options::parse_command_line( argc, argv, options ), args );
  boost::program_options::notify( args );

  if( args.count( "help" ) )
  {
    options.print( std::cout );
    return EXIT_SUCCESS;
  }

  if( args.count( "version" ) )
  {
    std::println( "v0.0.1" );
    return EXIT_SUCCESS;
  }

  std::uint16_t port                                                      = args[ "port" ].as< std::uint16_t >();
  std::optional< boost::asio::ip::tcp::resolver::results_type > endpoints = {};
  boost::asio::io_context ioc;

  if( args.count( "endpoint" ) )
  {
    const std::string endpoint_str = args[ "endpoint" ].as< std::string >();
    const auto colon_pos           = endpoint_str.rfind( ':' );

    if( colon_pos == std::string::npos )
    {
      std::cerr << "Invalid endpoint format. Expected: <host>:<port>\n";
      return EXIT_FAILURE;
    }

    const std::string host     = endpoint_str.substr( 0, colon_pos );
    const std::string port_str = endpoint_str.substr( colon_pos + 1 );

    try
    {
      boost::asio::ip::tcp::resolver resolver( ioc );
      endpoints = resolver.resolve( host, port_str );
    }
    catch( const boost::system::system_error& e )
    {
      std::cerr << "Invalid endpoint '" << endpoint_str << "': " << e.what() << "\n";
      return EXIT_FAILURE;
    }
  }

  std::string cert_path = args[ "cert" ].as< std::string >();
  std::string key_path  = args[ "key" ].as< std::string >();

  // Initialize app state
  app_state state;

  // Create custom TUI sink
  auto tui_sink = std::make_shared< TuiSink >( state );

  // Initialize logging with TUI sink
  respublica::log::initialize( { tui_sink } );

  // Create client
  respublica::net::client client( ioc, port, endpoints, cert_path, key_path );

  // Register chat message handler
  client.on_receive< chat_message >(
    [ &state ]( std::shared_ptr< respublica::net::peer > p, const chat_message& msg )
    {
      std::ostringstream oss;
      oss << "[" << respublica::net::peer_id_to_string( p->id() ).substr( 0, 8 ) << "]: " << msg.message;
      state.add_message( oss.str() );
    } );

  // Register peer event callbacks
  client.on_peer_connected(
    [ &state ]( respublica::net::peer_view p )
    {
      std::ostringstream oss;
      oss << "Peer connected: " << respublica::net::peer_id_to_string( p.id() ).substr( 0, 8 );
      state.add_message( oss.str() );
    } );

  client.on_peer_disconnected(
    [ &state ]( respublica::net::peer_id id, std::error_code ec )
    {
      std::ostringstream oss;
      oss << "Peer disconnected: " << respublica::net::peer_id_to_string( id ).substr( 0, 8 ) << " (" << ec.message()
          << ")";
      state.add_message( oss.str() );
    } );

  client.on_peer_state_change(
    [ &state ]( respublica::net::peer_view p,
                respublica::net::peer_state old_state,
                respublica::net::peer_state new_state )
    {
      std::ostringstream oss;
      oss << "Peer " << respublica::net::peer_id_to_string( p.id() ).substr( 0, 8 )
          << " state changed: " << static_cast< int >( old_state ) << " -> " << static_cast< int >( new_state );
      state.add_message( oss.str() );
    } );

  client.on_peer_reconnecting(
    [ &state ]( respublica::net::peer_view p, int attempt )
    {
      std::ostringstream oss;
      oss << "Peer " << respublica::net::peer_id_to_string( p.id() ).substr( 0, 8 ) << " reconnecting (attempt "
          << attempt << ")";
      state.add_message( oss.str() );
    } );

  // Run io_context in background thread
  std::thread io_thread(
    [ &ioc ]()
    {
      ioc.run();
    } );

  // Build TUI
  using namespace ftxui;

  auto screen = ScreenInteractive::Fullscreen();

  // Tab state
  int tab_index                        = 0;
  std::vector< std::string > tab_names = { "Messages", "Logs", "Peers" };

  // Input state
  std::string input_text;
  std::string target_peer_id; // Empty for broadcast
  bool broadcast_mode = true;

  // Input components
  auto input_component  = Input( &input_text, "Type a message..." );
  auto target_component = Input( &target_peer_id, "Peer ID (leave empty for broadcast)" )
                          | Maybe(
                            [ &broadcast_mode ]
                            {
                              return !broadcast_mode;
                            } );

  // Broadcast/Direct toggle
  auto toggle_broadcast = Checkbox( "Broadcast to all", &broadcast_mode );

  // Send button
  auto send_button = Button( "Send",
                             [ &client, &state, &input_text, &target_peer_id, &broadcast_mode, &screen ]()
                             {
                               if( input_text.empty() )
                                 return;

                               chat_message msg{ input_text };

                               if( broadcast_mode )
                               {
                                 client.broadcast( msg );
                                 state.add_message( "[You -> All]: " + input_text );
                               }
                               else
                               {
                                 // Parse peer ID and send
                                 if( target_peer_id.empty() )
                                 {
                                   state.add_message( "[Error] No peer ID specified for direct message" );
                                   return;
                                 }

                                 // Get all peers and find matching ID prefix
                                 auto peer_ids = client.get_peer_ids();
                                 std::optional< respublica::net::peer_id > matched_id;

                                 for( const auto& id: peer_ids )
                                 {
                                   std::string id_str = respublica::net::peer_id_to_string( id );
                                   if( id_str.starts_with( target_peer_id ) )
                                   {
                                     matched_id = id;
                                     break;
                                   }
                                 }

                                 if( !matched_id )
                                 {
                                   state.add_message( "[Error] Peer not found: " + target_peer_id );
                                   return;
                                 }

                                 auto ec = client.send( *matched_id, msg );
                                 if( ec )
                                 {
                                   state.add_message( "[Error] Failed to send: " + ec.message() );
                                 }
                                 else
                                 {
                                   state.add_message( "[You -> " + target_peer_id + "]: " + input_text );
                                 }
                               }

                               input_text.clear();
                               screen.Post( Event::Custom );
                             } );

  // Tab renderer
  auto tab_toggle = Toggle( &tab_names, &tab_index );

  // Messages tab
  auto messages_tab = Renderer(
    [ &state ]()
    {
      auto msgs     = state.get_messages();
      Elements list = { text( "Chat Messages" ) | bold | hcenter, separator() };

      if( msgs.empty() )
      {
        list.push_back( text( "No messages yet" ) | dim | center );
      }
      else
      {
        // Show last 50 messages
        size_t start = msgs.size() > 50 ? msgs.size() - 50 : 0;
        for( size_t i = start; i < msgs.size(); ++i )
        {
          list.push_back( text( msgs[ i ] ) );
        }
      }

      return vbox( std::move( list ) ) | frame | flex;
    } );

  // Logs tab
  auto logs_tab = Renderer(
    [ &state ]()
    {
      auto log_entries = state.get_logs();
      Elements list    = { text( "System Logs" ) | bold | hcenter, separator() };

      if( log_entries.empty() )
      {
        list.push_back( text( "No logs yet" ) | dim | center );
      }
      else
      {
        // Show last 100 logs
        size_t start = log_entries.size() > 100 ? log_entries.size() - 100 : 0;
        for( size_t i = start; i < log_entries.size(); ++i )
        {
          const auto& entry = log_entries[ i ];

          // Apply color based on log level
          Color log_color = Color::White;
          switch( entry.level )
          {
            case quill::LogLevel::Critical:
              log_color = Color::RedLight;
              break;
            case quill::LogLevel::Error:
              log_color = Color::Red;
              break;
            case quill::LogLevel::Warning:
              log_color = Color::Yellow;
              break;
            case quill::LogLevel::Info:
              log_color = Color::Green;
              break;
            case quill::LogLevel::Debug:
              log_color = Color::Cyan;
              break;
            case quill::LogLevel::TraceL1:
            case quill::LogLevel::TraceL2:
            case quill::LogLevel::TraceL3:
              log_color = Color::Blue;
              break;
            default:
              log_color = Color::White;
              break;
          }

          list.push_back( text( entry.message ) | color( log_color ) );
        }
      }

      return vbox( std::move( list ) ) | frame | flex;
    } );

  // Peers tab
  auto peers_tab = Renderer(
    [ &client ]()
    {
      auto peers = client.get_all_peers();

      Elements rows = { text( "Connected Peers" ) | bold | hcenter,
                        separator(),
                        hbox( { text( "Peer ID" ) | bold | size( WIDTH, EQUAL, 20 ),
                                separator(),
                                text( "State" ) | bold | size( WIDTH, EQUAL, 15 ),
                                separator(),
                                text( "Error Score" ) | bold | size( WIDTH, EQUAL, 12 ),
                                separator(),
                                text( "Reconnect" ) | bold | size( WIDTH, EQUAL, 10 ) } ),
                        separator() };

      if( peers.empty() )
      {
        rows.push_back( text( "No peers connected" ) | dim | center );
      }
      else
      {
        for( const auto& p: peers )
        {
          std::string state_str;
          switch( p.state() )
          {
            case respublica::net::peer_state::connecting:
              state_str = "Connecting";
              break;
            case respublica::net::peer_state::handshaking:
              state_str = "Handshaking";
              break;
            case respublica::net::peer_state::authenticating:
              state_str = "Authenticating";
              break;
            case respublica::net::peer_state::ready:
              state_str = "Ready";
              break;
            case respublica::net::peer_state::reconnecting:
              state_str = "Reconnecting";
              break;
            case respublica::net::peer_state::disconnected:
              state_str = "Disconnected";
              break;
            case respublica::net::peer_state::failed:
              state_str = "Failed";
              break;
          }

          Color state_color = Color::White;
          if( p.state() == respublica::net::peer_state::ready )
            state_color = Color::Green;
          else if( p.state() == respublica::net::peer_state::failed )
            state_color = Color::Red;
          else if( p.state() == respublica::net::peer_state::reconnecting )
            state_color = Color::Yellow;

          rows.push_back(
            hbox( { text( respublica::net::peer_id_to_string( p.id() ).substr( 0, 16 ) ) | size( WIDTH, EQUAL, 20 ),
                    separator(),
                    text( state_str ) | color( state_color ) | size( WIDTH, EQUAL, 15 ),
                    separator(),
                    text( std::to_string( p.error_score() ) ) | size( WIDTH, EQUAL, 12 ),
                    separator(),
                    text( std::to_string( p.reconnect_attempts() ) ) | size( WIDTH, EQUAL, 10 ) } ) );
        }
      }

      return vbox( std::move( rows ) ) | frame | flex;
    } );

  // Tab container
  auto tab_content = Container::Tab( { messages_tab, logs_tab, peers_tab }, &tab_index );

  // Input area
  auto input_area = Container::Vertical( { toggle_broadcast, target_component, input_component, send_button } );

  // Main container
  auto main_container = Container::Vertical( { tab_toggle, tab_content, input_area } );

  // Main renderer
  auto main_renderer = Renderer( main_container,
                                 [ &tab_toggle, &tab_content, &input_area, &port ]()
                                 {
                                   return vbox( {
                                     text( "Respublica P2P Chat" ) | bold | hcenter,
                                     text( "Port: " + std::to_string( port ) ) | hcenter | dim,
                                     separator(),
                                     tab_toggle->Render() | hcenter,
                                     separator(),
                                     tab_content->Render() | flex,
                                     separator(),
                                     input_area->Render() | size( HEIGHT, LESS_THAN, 8 ),
                                     separator(),
                                     text( "Press Ctrl+C to exit" ) | dim | hcenter,
                                   } );
                                 } );

  // Refresh every 200ms for live updates
  std::atomic< bool > refresh_ui = true;
  std::thread refresh_thread(
    [ &screen, &refresh_ui ]()
    {
      while( refresh_ui )
      {
        std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
        screen.Post( Event::Custom );
      }
    } );

  // Handle Ctrl+C gracefully
  screen.ExitLoopClosure();

  // Run the UI
  screen.Loop( main_renderer );

  // Cleanup
  refresh_ui = false;
  refresh_thread.join();

  ioc.stop();
  io_thread.join();

  return EXIT_SUCCESS;
}
