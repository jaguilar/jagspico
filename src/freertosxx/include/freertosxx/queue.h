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
      Receive(&item);
    }
  }

 protected:
  void Send(const void* item) {
    auto result = xQueueSend(queue_, item, portMAX_DELAY);
    configASSERT(result == pdTRUE);
  }

  bool TrySend(const void* item) { return SendWithTimeout(item, 0); }

  bool SendWithTimeout(const void* item, int ms) {
    return pdTRUE == xQueueSend(queue_, item, pdMS_TO_TICKS(ms));
  }

  bool SendFromISR(const void* item, bool& higher_priority_task_woken) {
    BaseType_t higher_priority_task_woken_raw = pdFALSE;
    const bool result =
        pdTRUE ==
        xQueueSendFromISR(this->queue_, item, &higher_priority_task_woken_raw);
    higher_priority_task_woken = pdTRUE == higher_priority_task_woken_raw;
    return result;
  }

  void Receive(void* item) {
    auto result = xQueueReceive(queue_, item, portMAX_DELAY);
    configASSERT(result == pdTRUE);
  }

  bool ReceiveWithTimeout(void* item, int ms) {
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

  void Send(const T& item) { UntypedQueue::Send(&item); }
  bool TrySend(const T& item) { return UntypedQueue::TrySend(&item); }
  void SendWithTimeout(int ms, const T& item) {
    UntypedQueue::SendWithTimeout(&item, ms);
  }
  bool SendFromISR(const T& item, bool& higher_priority_task_woken) {
    return pdTRUE ==
           UntypedQueue::SendFromISR(&item, higher_priority_task_woken);
  }
  T Receive() {
    T t;
    UntypedQueue::Receive(reinterpret_cast<void*>(&t));
    return t;
  }
  T ReceiveWithTimeout(int ms) {
    T t;
    UntypedQueue::ReceiveWithTimeout(reinterpret_cast<void*>(&t), ms);
    return t;
  }
};

template <typename T>
class DynamicQueue : public Queue<T> {
 public:
  DynamicQueue(int size) : Queue<T>(xQueueCreate(size, sizeof(T))) {}
};

template <typename T, int Size>
class StaticQueue : public Queue<T> {
 public:
  StaticQueue()
      : Queue<T>(xQueueCreateStatic(Size, sizeof(T), buf_, &queue_)) {}

 private:
  StaticQueue_t queue_;
  uint8_t buf_[Size * sizeof(T)];
};

}  // namespace freertosxx

#endif  // FREERTOSXX_QUEUE_H