
#include <koinos/exception.hpp>

namespace koinos {
namespace detail {

// nlohmann still has quotes around strings in json. For prettification, we want to
// remove those when replacing in a format string.
std::string trim_quotes( std::string&& s )
{
  if( s.size() >= 2 )
  {
    if( *( s.begin() ) == '"' )
      s.erase( 0, 1 );
    if( *( s.rbegin() ) == '"' )
      s.erase( s.size() - 1, s.size() );
  }

  return s;
}

std::string json_strpolate( const std::string& format_str, const nlohmann::json& j )
{
  std::string key;
  bool has_key;
  std::string result;

  auto maybe_get_key = [ & ]( const std::string& str, std::size_t start, std::size_t end ) -> std::size_t
  {
    // Consume at least one character
    // Return number of characters consumed
    // If key is found, set has_key, key
    has_key               = false;
    std::size_t key_start = start + 2;
    if( key_start >= end )
      return ( end - start );
    if( str[ start ] != '$' )
      return 1;
    if( str[ start + 1 ] != '{' )
      return 1;
    if( str[ key_start ] == '$' ) // Use ${$ to escape a literal ${
      return 3;
    for( std::size_t i = key_start; i < end; i++ )
    {
      if( str[ i ] == '}' )
      {
        has_key = true;
        key     = str.substr( key_start, i - key_start );
        return ( i - start ) + 1;
      }
    }
    return end - start;
  };

  std::size_t n = format_str.length();
  for( std::size_t i = 0; i < n; )
  {
    std::size_t consumed = maybe_get_key( format_str, i, n );
    if( has_key )
    {
      auto it = j.find( key );
      if( it != j.end() )
      {
        result += trim_quotes( it->dump() );
        i      += consumed;
        continue;
      }
    }
    for( std::size_t j = 0; j < consumed; j++ )
      result += format_str[ i + j ];
    i += consumed;
  }

  return result;
}

json_initializer::json_initializer( exception& e ):
    _e( e ),
    _j( *boost::get_error_info< koinos::detail::json_info >( e ) )
{}

json_initializer& json_initializer::operator()( const std::string& key, const google::protobuf::Message& m )
{
  google::protobuf::util::JsonPrintOptions options;
  options.add_whitespace             = true;
  options.preserve_proto_field_names = true;

  std::string json_str;
  [[maybe_unused]]
  auto errcode = google::protobuf::util::MessageToJsonString( m, &json_str, options );
  _j[ key ]    = nlohmann::json::parse( json_str.begin(), json_str.end() );
  _e.do_message_substitution();
  return *this;
}

json_initializer& json_initializer::operator()( const google::protobuf::Message& m )
{
  google::protobuf::util::JsonPrintOptions options;
  options.add_whitespace             = true;
  options.preserve_proto_field_names = true;

  std::string json_str;
  [[maybe_unused]]
  auto errcode = google::protobuf::util::MessageToJsonString( m, &json_str, options );
  _j.merge_patch( nlohmann::json::parse( json_str.begin(), json_str.end() ) );
  _e.do_message_substitution();
  return *this;
}

json_initializer& json_initializer::operator()( const std::string& key, const char* c )
{
  _j[ key ] = c;
  _e.do_message_substitution();
  return *this;
}

json_initializer& json_initializer::operator()()
{
  return *this;
}

} // namespace detail

exception::exception( int32_t c )
{
  *this << koinos::detail::json_info( nlohmann::json() );
  code = c;
}

exception::exception( int32_t c, const std::string& m ):
    exception()
{
  code = c;
  data.set_message( m );
}

exception::exception( int32_t c, std::string&& m ):
    exception()
{
  code = c;
  data.set_message( std::move( m ) );
}

exception::exception( const std::string& m ):
    exception()
{
  code = 1;
  data.set_message( m );
}

exception::exception( std::string&& m ):
    exception()
{
  code = 1;
  data.set_message( std::move( m ) );
}

exception::exception( int32_t c, const chain::error_data& d ):
    exception()
{
  code = c;
  data = d;
}

exception::exception( int32_t c, chain::error_data&& d ):
    exception()
{
  code = c;
  data = std::move( d );
}

exception::exception( const chain::error_data& d ):
    exception()
{
  code = 1;
  data = d;
}

exception::exception( chain::error_data&& d ):
    exception()
{
  code = 1;
  data = std::move( d );
}

exception::~exception() {}

const char* exception::what() const noexcept
{
  return data.message().c_str();
}

std::string exception::get_stacktrace() const
{
  std::string value;

  if( auto s = boost::get_error_info< koinos::detail::exception_stacktrace >( *this ); s != nullptr )
  {
    std::stringstream ss;
    ss << *s;
    value = ss.str();
  }

  return value;
}

const nlohmann::json& exception::get_json() const
{
  return *boost::get_error_info< koinos::detail::json_info >( *this );
}

const std::string& exception::get_message() const
{
  return data.message();
}

const chain::error_data& exception::get_data() const
{
  return data;
}

void exception::do_message_substitution()
{
  data.set_message(
    detail::json_strpolate( get_message(), *boost::get_error_info< koinos::detail::json_info >( *this ) ) );
}

} // namespace koinos
