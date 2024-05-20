#ifndef JAGSPICO_UTIL_MUTEX_H
#define JAGSPICO_UTIL_MUTEX_H

#include <FreeRTOSConfig.h>

#include "FreeRTOS.h"
#include "projdefs.h"
#include "semphr.h"

namespace freertosxx {

// Wraps a FreeRTOS mutex in an ABSL-like interface.
class Mutex {
 public:
  Mutex();
  ~Mutex();
  Mutex(const Mutex&) = delete;
  Mutex(Mutex&&) = delete;
  Mutex& operator=(const Mutex&) = delete;
  Mutex& operator=(Mutex&&) = delete;

  void Lock() {
    auto result = xSemaphoreTake(mutex_, portMAX_DELAY);
    configASSERT(result == pdTRUE);
  }

  void Unlock() {
    configASSERT(xSemaphoreGetMutexHolder(mutex_) ==
                 xTaskGetCurrentTaskHandle());
    xSemaphoreGive(mutex_);
  }

 private:
  StaticSemaphore_t storage_;
  SemaphoreHandle_t mutex_;
};

class MutexLock {
 public:
  explicit MutexLock(Mutex& mutex) : mutex_(mutex) { mutex_.Lock(); }
  ~MutexLock() { mutex_.Unlock(); }
  MutexLock(const MutexLock&) = delete;
  MutexLock(MutexLock&&) = delete;
  MutexLock& operator=(const MutexLock&) = delete;
  MutexLock& operator=(MutexLock&&) = delete;

 private:
  Mutex& mutex_;
};

}  // namespace freertosxx

#endif  // JAGSPICO_UTIL_MUTEX_H