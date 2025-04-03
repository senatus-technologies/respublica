#pragma once

#include <chrono>

namespace koinos::timer::detail {

void timer_log();

struct timer {
  timer(const std::string& label) :
    start( std::chrono::steady_clock::now() ),
    label( label )
  {}

  ~timer();

  std::chrono::steady_clock::time_point start;
  const std::string label;
};

} // koinos::timer::detail

#define KOINOS_TIMING

#ifdef KOINOS_TIMING
#define KOINOS_TIMER(label) koinos::timer::detail::timer __timer( label );
#define KOINOS_TIMER_LOG() koinos::timer::detail::timer_log();
#else
#define KOINOS_TIMER(label)
#define KOINOS_TIMER_LOG()
#endif
