#include "util/ssprintf.h"

#include <string>

#include "pico/printf.h"
#include "pico/stdio.h"

using namespace jagspico;

int main() {
  stdio_init_all();

  std::string s;
  ssappendf(s, "hello %d", 42);

  if (s != "hello 42") {
    panic("FAIL: want: hello 42 got: %s", s.c_str());
  }

  s = std::string("abc ");
  ssappendf(s, "hello %d", 42);
  if (s != "abc hello 42") {
    panic("FAIL: abc hello 42");
  }

  // Verify that we grow the string properly.
  s = std::string();
  const int copies_of_underscore = s.capacity() / 2;
  s.resize(copies_of_underscore);
  for (auto& c : s) c = ' ';

  const int copies_of_42 = s.capacity() - s.size();
  for (int i = 0; i < copies_of_42; ++i) {
    ssappendf(s, "%d", 42);
  }
  if (!s.starts_with(std::string(copies_of_underscore, ' '))) {
    panic("doesn't start with appropriate amount of underscores");
  }
  std::string_view rest(s);
  rest.remove_prefix(copies_of_underscore);
  for (int i = 0; i < copies_of_42; ++i) {
    if (!rest.starts_with("42")) {
      panic("doesn't start with 42");
    }
    rest.remove_prefix(2);
  }
  if (!rest.empty()) {
    panic("extra characters at the end!: %s", rest.data());
  }
  printf("PASS\n");
}