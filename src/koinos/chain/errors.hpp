#pragma once
#include <system_error>

namespace koinos::chain {

//enum class error_code : int64_t
//{
//
//};

struct koinos_error_category : public std::error_category
{
  koinos_error_category() = default;
  koinos_error_category( std::string_view );

  const char* name() const noexcept override;
  std::string message( int ) const override;

  private:
    std::string_view _message;
};

struct koinos_error : public std::error_code
{
  koinos_error() noexcept : std::error_code()
  {}

  koinos_error( int ec ) noexcept : std::error_code( ec, _category )
  {}

  private:
    static const koinos_error_category _category();
};

bool is_failure( const koinos_error& );
bool is_reversion( const koinos_error& );

} // namespace koinos::chain

#define KOINOS_CHECK_ERROR_3( predicate, error_code, message )                                         \
do                                                                                                     \
{                                                                                                      \
  if( !( predicate ) )                                                                                 \
    return koinos::chain::koinos_error( error_code, koinos::chain::koinos_error_category( message ) ); \
} while( 0 )

#define KOINOS_CHECK_ERROR_2( predicate, error_code )                                         \
do                                                                                            \
{                                                                                             \
  if( !( predicate ) )                                                                        \
    return koinos::chain::koinos_error( error_code, koinos::chain::koinos_error_category() ); \
} while( 0 )

#define KOINOS_CHECK_ERROR( ... ) BOOST_PP_OVERLOAD( KOINOS_CHECK_ERROR_, __VA_ARGS__ )( __VA_ARGS__ )
