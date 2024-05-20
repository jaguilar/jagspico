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

  bool TryLock() { return pdTRUE == xSemaphoreTake(mutex_, 0); }

  bool LockWithTimeout(int ms) {
    return pdTRUE == xSemaphoreTake(mutex_, pdMS_TO_TICKS(ms));
  }

  void Unlock() {
    configASSERT(xSemaphoreGetMutexHolder(mutex_) ==
                 xTaskGetCurrentTaskHandle());
    xSemaphoreGive(mutex_);
  }

  bool LockFromISR(bool& higher_priority_task_woken) {
    BaseType_t higher_priority_task_woken_raw = pdFALSE;
    const bool result = pdTRUE == xSemaphoreTakeFromISR(
                                      mutex_, &higher_priority_task_woken_raw);
    higher_priority_task_woken = pdTRUE == higher_priority_task_woken_raw;
    return result;
  }

  void UnlockFromISR(bool& higher_priority_task_woken) {
    BaseType_t higher_priority_task_woken_raw = pdFALSE;
    configASSERT(xSemaphoreGetMutexHolder(mutex_) ==
                 xTaskGetCurrentTaskHandle());
    xSemaphoreGiveFromISR(mutex_, &higher_priority_task_woken_raw);
    higher_priority_task_woken = pdTRUE == higher_priority_task_woken_raw;
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