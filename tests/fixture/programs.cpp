#include <test/programs.hpp>

#include <test/wasm/koin.hpp>

#define DEFINE_WASM( program_name )                                                                                    \
  const std::vector< std::byte >& program_name##_program()                                                             \
  {                                                                                                                    \
    static std::vector< std::byte > wasm;                                                                              \
    if( !wasm.size() )                                                                                                 \
      for( size_t i = 0; i < program_name##_wasm_len; i++ )                                                            \
        wasm.push_back( std::byte( program_name##_wasm[ i ] ) );                                                       \
    return wasm;                                                                                                       \
  }

DEFINE_WASM( koin )
