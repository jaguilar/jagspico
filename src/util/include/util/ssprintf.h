// A library that sprintfs correctly onto a C++ string.
//
// The compiler I'm using doesn't have access to std::format,
// sprintf is a pain to use in C++, and std::ostringstream
// costs 250kB! Meanwhile, in release mode, a call to this function
// adds only 50 bytes or so compared to calling printf.

#ifndef JAGSPICO_UTIL_SSPRINTF_H
#define JAGSPICO_UTIL_SSPRINTF_H

#include <cstdio>
#include <string>

#include "pico/platform.h"

namespace jagspico {

// Calls sprintf, but prints into a dynamically allocated string.
template <typename... Args>
std::string ssprintf(const char* fmt, Args&&... args);

// Calls sprintf, but appends onto a dynamically allocated string.
// The string will be grown exactly to the length of the original
// contents plus the formatted string, but the capacity may be increased
// beyond that.
template <typename... Args>
void ssappendf(std::string& s, const char* fmt, Args&&...);

template <typename... Args>
std::string ssprintf(const char* fmt, Args&&... args) {
  std::string s;
  ssappendf(s, fmt, std::forward<Args>(args)...);
  return s;
}

template <typename... Args>
void ssappendf(std::string& s, const char* fmt, Args&&... args) {
  const std::size_t size_before = s.size();
  s.resize(s.capacity());
  const std::size_t print_capacity = s.size() - size_before;
  // We have to make a little fib to snprintf. The C++ buffer always ends with
  // nul, but that nul is not included in the capacity of the buffer. snprintf
  // will write a terminating nul instead of a desired character if it thinks
  // it's out of space. So we tell it there's one more spot than there really
  // is, understanding that snprintf will do no harm here other than writing
  // a null, which is what's already there.
  const int chars_to_print =
      snprintf(&s.data()[size_before], print_capacity + 1, fmt, args...);
  if (chars_to_print < 0) {
    panic("encoding error during snprintf %s", fmt);
  }
  s.resize(size_before + chars_to_print);
  if (chars_to_print > print_capacity) {
    // The print had to stop early last time, so we need to redo the print.
    snprintf(&s.data()[size_before], chars_to_print + 1, fmt, args...);
  }
}

}  // namespace jagspico

#endif