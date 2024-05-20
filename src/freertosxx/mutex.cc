#include "freertosxx/mutex.h"

namespace freertosxx {

Mutex::Mutex() { mutex_ = xSemaphoreCreateMutexStatic(&storage_); }

Mutex::~Mutex() {
  configASSERT(xSemaphoreGetMutexHolder(mutex_) == NULL);
  vSemaphoreDelete(mutex_);
}

}  // namespace freertosxx