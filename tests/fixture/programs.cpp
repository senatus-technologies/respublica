// NOLINTBEGIN

#include <test/programs.hpp>

#include <test/wasm/token.hpp>

#define DEFINE_WASM( program_name )                                                                                    \
  const std::vector< std::byte >& program_name##_program()                                                             \
  {                                                                                                                    \
    static std::vector< std::byte > wasm;                                                                              \
    if( !wasm.size() )                                                                                                 \
      for( std::size_t i = 0; i < program_name##_len; i++ )                                                            \
        wasm.push_back( std::byte( program_name[ i ] ) );                                                              \
    return wasm;                                                                                                       \
  }

DEFINE_WASM( token )

// NOLINTEND
