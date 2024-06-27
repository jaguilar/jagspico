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
  std::size_t size_before = s.size();
  printf("%*s %d\n", s.size(), s.c_str(), s.size());
  s.resize(s.capacity());
  const std::size_t print_capacity = s.size() - size_before;
  const int want_cap =
      snprintf(&s.data()[size_before], print_capacity, fmt, args...) + 1;
  printf("%*s %d\n", s.size(), s.c_str(), s.size());
  if (want_cap >= print_capacity) {
    // The print had to stop early. Resize the string and print again.
    s.resize(size_before + want_cap);
    snprintf(&s.data()[size_before], want_cap, fmt, args...);
    printf("reprinted: ");
    printf(fmt, args...);
    printf("\n%*s\n", s.size(), s.c_str());
  }
  s[size_before + want_cap] = '\0';
  s.resize(size_before + want_cap - 1);
  printf("would have printed: ");
  printf(fmt, args...);
  printf("\n%*s\n", s.size(), s.c_str());
}

}  // namespace jagspico

#endif