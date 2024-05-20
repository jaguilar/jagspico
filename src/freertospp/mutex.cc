#include "jagspico/mutex.h"

namespace jagspico {

Mutex::Mutex() { mutex_ = xSemaphoreCreateMutex(); }

Mutex::~Mutex() {
  configASSERT(xSemaphoreGetMutexHolder(mutex_) == NULL);
  vSemaphoreDelete(mutex_);
}

}  // namespace jagspico