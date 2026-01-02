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
class tui_sink: public quill::Sink
{
public:
  explicit tui_sink( app_state& state ):
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

  // Initialize logging with TUI sink
  respublica::log::initialize( { std::make_shared< tui_sink >( state ) } );

  // Create client
  auto client = std::make_unique< respublica::net::client >( ioc, port, endpoints, cert_path, key_path );

  // Register chat message handler
  client->on_receive< chat_message >(
    [ &state ]( const respublica::net::peer_view& p, const chat_message& msg )
    {
      constexpr std::size_t display_id_len = 8;
      state.add_message( std::format( "[{}]: {}",
                                      respublica::net::peer_id_to_string( p.id() ).substr( 0, display_id_len ),
                                      msg.message ) );
    } );

  // Run io_context in background thread
  std::thread io_thread(
    [ &ioc ]()
    {
      ioc.run();
    } );

  auto screen = ftxui::ScreenInteractive::Fullscreen();

  // Tab state
  int tab_index                        = 0;
  std::vector< std::string > tab_names = { "Messages", "Logs", "Peers" };

  // Command input state
  std::string command_input;
  std::string command_status; // Feedback for last command

  // Command handler
  auto execute_command = [ &client, &state, &command_input, &command_status, &screen, &ioc ]()
  {
    if( command_input.empty() )
      return;

    // Parse command
    std::istringstream iss( command_input );
    std::string cmd;
    iss >> cmd;

    if( cmd.empty() || cmd[ 0 ] != '/' )
    {
      command_status = "[Error] Commands must start with /. Type /help for available commands.";
      command_input.clear();
      screen.Post( ftxui::Event::Custom );
      return;
    }

    // Remove leading slash
    cmd = cmd.substr( 1 );

    if( cmd == "help" )
    {
      state.add_message( "Available commands:" );
      state.add_message( "  /send <peer_id> <message> - Send message to specific peer" );
      state.add_message( "  /broadcast <message> - Broadcast message to all peers" );
      state.add_message( "  /connect <host>:<port> - Connect to a peer" );
      state.add_message( "  /disconnect <peer_id> - Disconnect from a peer" );
      state.add_message( "  /exit - Exit the application" );
      state.add_message( "  /help - Show this help message" );
      command_status = "Help displayed";
    }
    else if( cmd == "send" )
    {
      std::string peer_id_str;
      iss >> peer_id_str;

      if( peer_id_str.empty() )
      {
        command_status = "[Error] Usage: /send <peer_id> <message>";
      }
      else
      {
        std::string message;
        std::getline( iss, message );
        if( !message.empty() && message[ 0 ] == ' ' )
          message = message.substr( 1 );

        if( message.empty() )
        {
          command_status = "[Error] Message cannot be empty";
        }
        else
        {
          // Find matching peer
          auto peer_ids = client->get_peer_ids();
          std::optional< respublica::net::peer_id > matched_id;

          for( const auto& id: peer_ids )
          {
            std::string id_str = respublica::net::peer_id_to_string( id );
            if( id_str.starts_with( peer_id_str ) )
            {
              matched_id = id;
              break;
            }
          }

          if( !matched_id )
          {
            command_status = "[Error] Peer not found: " + peer_id_str;
          }
          else
          {
            chat_message msg{ message };
            auto ec = client->send( *matched_id, msg );
            if( ec )
            {
              command_status = "[Error] Failed to send: " + ec.message();
            }
            else
            {
              state.add_message( "[You -> " + peer_id_str + "]: " + message );
              command_status = "Message sent to " + peer_id_str;
            }
          }
        }
      }
    }
    else if( cmd == "broadcast" )
    {
      std::string message;
      std::getline( iss, message );
      if( !message.empty() && message[ 0 ] == ' ' )
        message = message.substr( 1 );

      if( message.empty() )
      {
        command_status = "[Error] Usage: /broadcast <message>";
      }
      else
      {
        chat_message msg{ message };
        client->broadcast( msg );
        state.add_message( "[You -> All]: " + message );
        command_status = "Message broadcasted";
      }
    }
    else if( cmd == "connect" )
    {
      std::string endpoint_str;
      iss >> endpoint_str;

      if( endpoint_str.empty() )
      {
        command_status = "[Error] Usage: /connect <host>:<port>";
      }
      else
      {
        const auto colon_pos = endpoint_str.rfind( ':' );
        if( colon_pos == std::string::npos )
        {
          command_status = "[Error] Invalid endpoint format. Expected: <host>:<port>";
        }
        else
        {
          const std::string host     = endpoint_str.substr( 0, colon_pos );
          const std::string port_str = endpoint_str.substr( colon_pos + 1 );

          try
          {
            boost::asio::ip::tcp::resolver resolver( ioc );
            auto endpoints = resolver.resolve( host, port_str );
            client->connect( endpoints );
            command_status = "Connecting to " + endpoint_str;
            state.add_message( "[System] Attempting to connect to " + endpoint_str );
          }
          catch( const boost::system::system_error& e )
          {
            command_status = "[Error] Invalid endpoint: " + std::string( e.what() );
          }
        }
      }
    }
    else if( cmd == "disconnect" )
    {
      std::string peer_id_str;
      iss >> peer_id_str;

      if( peer_id_str.empty() )
      {
        command_status = "[Error] Usage: /disconnect <peer_id>";
      }
      else
      {
        // Find matching peer
        auto peer_ids = client->get_peer_ids();
        std::optional< respublica::net::peer_id > matched_id;

        for( const auto& id: peer_ids )
        {
          std::string id_str = respublica::net::peer_id_to_string( id );
          if( id_str.starts_with( peer_id_str ) )
          {
            matched_id = id;
            break;
          }
        }

        if( !matched_id )
        {
          command_status = "[Error] Peer not found: " + peer_id_str;
        }
        else
        {
          client->disconnect( *matched_id );
          command_status = "Disconnected from " + peer_id_str;
          state.add_message( "[System] Disconnected from " + peer_id_str );
        }
      }
    }
    else if( cmd == "exit" )
    {
      screen.Exit();
      return;
    }
    else
    {
      command_status = "[Error] Unknown command: /" + cmd + ". Type /help for available commands.";
    }

    command_input.clear();
    screen.Post( ftxui::Event::Custom );
  };

  // Command input component with Enter to execute
  auto command_component          = ftxui::Input( &command_input, "Type a command (e.g., /help)..." );
  auto command_input_with_handler = ftxui::CatchEvent( command_component,
                                                       [ &execute_command ]( ftxui::Event event )
                                                       {
                                                         if( event == ftxui::Event::Return )
                                                         {
                                                           execute_command();
                                                           return true;
                                                         }
                                                         return false;
                                                       } );

  // Tab renderer with horizontal animated menu (like FTXUI demo)
  auto tab_toggle = ftxui::Menu( &tab_names, &tab_index, ftxui::MenuOption::HorizontalAnimated() );

  // Messages tab
  auto messages_tab = ftxui::Renderer(
    [ &state ]()
    {
      auto msgs            = state.get_messages();
      ftxui::Elements list = {};

      if( msgs.empty() )
      {
        list.push_back( ftxui::text( "No messages yet" ) | ftxui::dim | ftxui::center );
      }
      else
      {
        // Show last 50 messages
        constexpr std::size_t max_messages = 50;
        std::size_t start                  = msgs.size() > max_messages ? msgs.size() - max_messages : 0;
        for( std::size_t i = start; i < msgs.size(); ++i )
        {
          list.push_back( ftxui::text( msgs[ i ] ) );
        }
      }

      return ftxui::vbox( std::move( list ) ) | ftxui::frame | ftxui::flex;
    } );

  // Logs tab
  auto logs_tab = ftxui::Renderer(
    [ &state ]()
    {
      auto log_entries     = state.get_logs();
      ftxui::Elements list = {};

      if( log_entries.empty() )
      {
        list.push_back( ftxui::text( "No logs yet" ) | ftxui::dim | ftxui::center );
      }
      else
      {
        // Show last 100 logs
        const std::size_t max_logs = 100;
        std::size_t start          = log_entries.size() > max_logs ? log_entries.size() - max_logs : 0;
        for( std::size_t i = start; i < log_entries.size(); ++i )
        {
          const auto& entry = log_entries[ i ];

          // Apply color based on log level
          ftxui::Color log_color = ftxui::Color::White;
          switch( entry.level )
          {
            case quill::LogLevel::Critical:
              log_color = ftxui::Color::RedLight;
              break;
            case quill::LogLevel::Error:
              log_color = ftxui::Color::Red;
              break;
            case quill::LogLevel::Warning:
              log_color = ftxui::Color::Yellow;
              break;
            case quill::LogLevel::Info:
              log_color = ftxui::Color::Green;
              break;
            case quill::LogLevel::Debug:
              log_color = ftxui::Color::Cyan;
              break;
            case quill::LogLevel::TraceL1:
            case quill::LogLevel::TraceL2:
            case quill::LogLevel::TraceL3:
              log_color = ftxui::Color::Blue;
              break;
            default:
              log_color = ftxui::Color::White;
              break;
          }

          list.push_back( ftxui::text( entry.message ) | ftxui::color( log_color ) );
        }
      }

      return ftxui::vbox( std::move( list ) ) | ftxui::frame | ftxui::flex;
    } );

  // Peers tab
  auto peers_tab = ftxui::Renderer(
    [ &client ]()
    {
      auto peers                     = client->get_all_peers();
      constexpr int peer_id_size     = 20;
      constexpr int state_size       = 15;
      constexpr int error_score_size = 12;
      constexpr int reconnect_size   = 10;

      ftxui::Elements rows = {
        ftxui::hbox(
          { ftxui::text( "Peer ID" ) | ftxui::bold | ftxui::size( ftxui::WIDTH, ftxui::EQUAL, peer_id_size ),
            ftxui::separator(),
            ftxui::text( "State" ) | ftxui::bold | ftxui::size( ftxui::WIDTH, ftxui::EQUAL, state_size ),
            ftxui::separator(),
            ftxui::text( "Error Score" ) | ftxui::bold | ftxui::size( ftxui::WIDTH, ftxui::EQUAL, error_score_size ),
            ftxui::separator(),
            ftxui::text( "Reconnect" ) | ftxui::bold | ftxui::size( ftxui::WIDTH, ftxui::EQUAL, reconnect_size ) } ),
        ftxui::separator() };

      if( peers.empty() )
      {
        rows.push_back( ftxui::text( "No peers connected" ) | ftxui::dim | ftxui::center );
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

          ftxui::Color state_color = ftxui::Color::White;
          if( p.state() == respublica::net::peer_state::ready )
            state_color = ftxui::Color::Green;
          else if( p.state() == respublica::net::peer_state::failed )
            state_color = ftxui::Color::Red;
          else if( p.state() == respublica::net::peer_state::reconnecting )
            state_color = ftxui::Color::Yellow;

          constexpr std::size_t peer_id_display_len = 16;
          constexpr int peer_id_size                = 20;
          constexpr int state_size                  = 15;
          constexpr int error_score_size            = 12;
          constexpr int reconnect_size              = 10;

          rows.push_back(
            ftxui::hbox( { ftxui::text( respublica::net::peer_id_to_string( p.id() ).substr( 0, peer_id_display_len ) )
                             | ftxui::size( ftxui::WIDTH, ftxui::EQUAL, peer_id_size ),
                           ftxui::separator(),
                           ftxui::text( state_str ) | ftxui::color( state_color )
                             | ftxui::size( ftxui::WIDTH, ftxui::EQUAL, state_size ),
                           ftxui::separator(),
                           ftxui::text( std::to_string( p.error_score() ) )
                             | ftxui::size( ftxui::WIDTH, ftxui::EQUAL, error_score_size ),
                           ftxui::separator(),
                           ftxui::text( std::to_string( p.reconnect_attempts() ) )
                             | ftxui::size( ftxui::WIDTH, ftxui::EQUAL, reconnect_size ) } ) );
        }
      }

      return ftxui::vbox( std::move( rows ) ) | ftxui::frame | ftxui::flex;
    } );

  // Tab container
  auto tab_content = ftxui::Container::Tab( { messages_tab, logs_tab, peers_tab }, &tab_index );

  // Command input area
  auto command_area = ftxui::Container::Vertical( { command_input_with_handler } );

  // Main container
  auto main_container = ftxui::Container::Vertical( { tab_toggle, tab_content, command_area } );

  // Main renderer
  auto main_renderer =
    Renderer( main_container,
              [ &tab_toggle, &tab_content, &command_area, &command_status, &port ]()
              {
                return ftxui::vbox( {
                  ftxui::text( "Respublica Network Client" ) | ftxui::bold | ftxui::hcenter,
                  ftxui::text( "Port: " + std::to_string( port ) ) | ftxui::hcenter | ftxui::dim,
                  ftxui::separator(),
                  tab_toggle->Render(),
                  ftxui::separator(),
                  tab_content->Render() | ftxui::flex,
                  ftxui::separator(),
                  ftxui::vbox( {
                    command_area->Render(),
                    command_status.empty() ? ftxui::text( "Type /help for available commands" ) | ftxui::dim
                                           : ftxui::text( command_status ),
                  } ),
                } );
              } );

  // Refresh every 200ms for live updates
  std::atomic< bool > refresh_ui = true;
  std::thread refresh_thread(
    [ &screen, &refresh_ui ]()
    {
      while( refresh_ui )
      {
        constexpr auto refresh_rate = std::chrono::milliseconds( 200 );
        std::this_thread::sleep_for( refresh_rate );
        screen.Post( ftxui::Event::Custom );
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
  client.reset();

  return EXIT_SUCCESS;
}
