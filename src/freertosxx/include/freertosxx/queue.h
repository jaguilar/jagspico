#ifndef FREERTOSXX_QUEUE_H
#define FREERTOSXX_QUEUE_H

#include "FreeRTOS.h"
#include "freertosxx/mutex.h"
#include "portmacro.h"
#include "projdefs.h"
namespace freertosxx {

template <size_t length, size_t itemsize>
class QueueStaticBase {
 public:
  QueueStaticBase()
      : queue_(xQueueCreateStatic(length, itemsize, buf_, &storage_)) {}
  virtual ~QueueStaticBase() {}
  QueueStaticBase(const QueueStaticBase&) = delete;
  QueueStaticBase(QueueStaticBase&&) = delete;
  QueueStaticBase& operator=(const QueueStaticBase&) = delete;
  QueueStaticBase& operator=(QueueStaticBase&&) = delete;

  void Drain() {
    while (uxQueueMessagesWaiting(queue_) > 0) {
      void* item;
      Pop(&item);
    }
  }

 protected:
  void Push(const void* item) {
    auto result = xQueueSend(queue_, item, portMAX_DELAY);
    configASSERT(result == pdTRUE);
  }

  bool PushWithTimeout(const void* item, int ms) {
    return pdTRUE == xQueueSend(queue_, item, pdMS_TO_TICKS(ms));
  }

  bool PushFromISR(const void* item, bool& higher_priority_task_woken) {
    BaseType_t higher_priority_task_woken_raw = pdFALSE;
    const bool result =
        pdTRUE ==
        xQueueSendFromISR(this->queue_, item, &higher_priority_task_woken_raw);
    higher_priority_task_woken = pdTRUE == higher_priority_task_woken_raw;
    return result;
  }

  void Pop(void* item) {
    auto result = xQueueReceive(queue_, item, portMAX_DELAY);
    configASSERT(result == pdTRUE);
  }

  bool PopWithTimeout(void* item, int ms) {
    return pdTRUE == xQueueReceive(queue_, item, pdMS_TO_TICKS(ms));
  }

  QueueHandle_t queue_;

 private:
  StaticQueue_t storage_;
  uint8_t buf_[length * itemsize];
};

template <typename T, size_t length>
class Queue : public QueueStaticBase<length, sizeof(T)> {
 public:
  Queue() = default;
  virtual ~Queue() = default;

  void Push(const T& item) { Push(reinterpret_cast<const void*>(&item)); }
  void PushWithTimeout(int ms, const T& item) {
    PushWithTimeout(reinterpret_cast<const void*>(&item), ms);
  }
  bool PushFromISR(const T& item, bool& higher_priority_task_woken) {
    return pdTRUE == PushFromISR(&item, higher_priority_task_woken);
  }
  T Pop() {
    T t;
    QueueStaticBase<length, sizeof(T)>::Pop(reinterpret_cast<void*>(&t));
    return t;
  }
  T PopWithTimeout(int ms) {
    T t;
    PopWithTimeout(reinterpret_cast<void*>(&t), ms);
    return t;
  }
};

}  // namespace freertosxx

#endif  // FREERTOSXX_QUEUE_H