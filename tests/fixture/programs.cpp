#include <test/programs.hpp>

#include <test/wasm/token.hpp>

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
#define DEFINE_WASM( program_name )                                                                                    \
  const std::vector< std::byte >& program_name##_program()                                                             \
  {                                                                                                                    \
    static std::vector< std::byte > wasm;                                                                              \
    if( !wasm.size() )                                                                                                 \
      for( std::size_t i = 0; i < program_name##_len; i++ )                                                            \
        wasm.push_back( std::byte( program_name[ i ] ) );                                                              \
    return wasm;                                                                                                       \
  }

// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

DEFINE_WASM( token )
