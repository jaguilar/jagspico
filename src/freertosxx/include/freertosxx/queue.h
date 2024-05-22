#ifndef FREERTOSXX_QUEUE_H
#define FREERTOSXX_QUEUE_H

#include <type_traits>
#include "FreeRTOS.h"
#include "freertosxx/mutex.h"
#include "portmacro.h"
#include "projdefs.h"
namespace freertosxx {

class UntypedQueue {
 public:
  UntypedQueue(QueueHandle_t queue) : queue_(queue) {}
  ~UntypedQueue() { vQueueDelete(queue_); }
  UntypedQueue(const UntypedQueue&) = delete;
  UntypedQueue(UntypedQueue&&) = delete;
  UntypedQueue& operator=(const UntypedQueue&) = delete;
  UntypedQueue& operator=(UntypedQueue&&) = delete;

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
};

template <typename T>
class Queue : public UntypedQueue {
 public:
  static_assert(
      std::is_trivial_v<T> && std::is_standard_layout_v<T>,
      "T must be a POD type");
  Queue(QueueHandle_t queue) : UntypedQueue(queue) {}

  void Push(const T& item) { Push(reinterpret_cast<const void*>(&item)); }
  void PushWithTimeout(int ms, const T& item) {
    PushWithTimeout(reinterpret_cast<const void*>(&item), ms);
  }
  bool PushFromISR(const T& item, bool& higher_priority_task_woken) {
    return pdTRUE == PushFromISR(&item, higher_priority_task_woken);
  }
  T Pop() {
    T t;
    UntypedQueue::Pop(reinterpret_cast<void*>(&t));
    return t;
  }
  T PopWithTimeout(int ms) {
    T t;
    PopWithTimeout(reinterpret_cast<void*>(&t), ms);
    return t;
  }
};

template <typename T>
class RTOSDynamicQueue : public Queue<T> {
 public:
  RTOSDynamicQueue(int size) : Queue<T>(xQueueCreate(size, sizeof(T))) {}
};

template <typename T, int Size>
class RTOSStaticQueue : public Queue<T> {
 public:
  RTOSStaticQueue()
      : Queue<T>(xQueueCreateStatic(Size, sizeof(T), buf_, &queue_)) {}

 private:
  StaticQueue_t queue_;
  uint8_t buf_[Size * sizeof(T)];
};

}  // namespace freertosxx

#endif  // FREERTOSXX_QUEUE_H