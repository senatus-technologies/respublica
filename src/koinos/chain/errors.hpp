#pragma once
#include <system_error>

namespace koinos::chain {

//enum class error_code : int64_t
//{
//
//};

struct error_category : public std::error_category
{
  error_category() = default;
  error_category( std::string_view );

  const char* name() const noexcept override;
  std::string message( int ) const override;

  private:
    std::string_view _message;
};

struct error_code : public std::error_code
{
  error_code( int ec ) noexcept : std::error_code( ec, _category )
  {}

  error_code( int ec, std::string_view msg ) : std::error_code( ec, error_category( msg ) )
  {}

  bool is_failure() const;
  bool is_reversion() const;

  private:
    static const error_category _category;
};

bool is_failure( const error_code& );
bool is_reversion( const error_code& );

} // namespace koinos::chain

#define KOINOS_CHECK_ERROR_3( predicate, error_code, message ) \
do                                                             \
{                                                              \
  if( !( predicate ) )                                         \
    return koinos::chain::error_code( error_code, message );   \
} while( 0 )

#define KOINOS_CHECK_ERROR_2( predicate, error_code ) \
do                                                    \
{                                                     \
  if( !( predicate ) )                                \
    return koinos::chain::error_code( error_code );   \
} while( 0 )

#define KOINOS_CHECK_ERROR( ... ) BOOST_PP_OVERLOAD( KOINOS_CHECK_ERROR_, __VA_ARGS__ )( __VA_ARGS__ )
