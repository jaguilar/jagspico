#include "freertosxx/mutex.h"

#include <utility>

namespace freertosxx {

Mutex::Mutex() { mutex_ = xSemaphoreCreateMutex(); }

Mutex::~Mutex() {
  if (mutex_ == nullptr) return;
  configASSERT(xSemaphoreGetMutexHolder(mutex_) == NULL);
  vSemaphoreDelete(mutex_);
}

Mutex::Mutex(Mutex&& o) { *this = std::forward<Mutex&&>(o); }

Mutex& Mutex::operator=(Mutex&& o) {
  configASSERT(xSemaphoreGetMutexHolder(o.mutex_) == nullptr);
  mutex_ = std::exchange(o.mutex_, nullptr);
  return *this;
}
}  // namespace freertosxx