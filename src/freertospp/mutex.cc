#include "freertospp/mutex.h"

namespace jagspico {

Mutex::Mutex() { mutex_ = xSemaphoreCreateMutexStatic(&storage_); }

Mutex::~Mutex() {
  configASSERT(xSemaphoreGetMutexHolder(mutex_) == NULL);
  vSemaphoreDelete(mutex_);
}

}  // namespace jagspico