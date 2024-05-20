#ifndef JAGSPICO_CLEANUP_H
#define JAGSPICO_CLEANUP_H

#include <utility>

namespace jagspico {

template <typename F>
class Cleanup {
 public:
  explicit Cleanup(F&& f) : f_(std::forward<F>(f)) {}
  ~Cleanup() { f_(); }
  Cleanup(const Cleanup&) = delete;
  Cleanup(Cleanup&&) = delete;
  Cleanup& operator=(const Cleanup&) = delete;
  Cleanup& operator=(Cleanup&&) = delete;

 private:
  F f_;
};

}  // namespace jagspico

#endif