#ifndef FREERTOSXX_EVENT_H
#define FREERTOSXX_EVENT_H

#include <optional>

#include "FreeRTOS.h"
#include "event_groups.h"
#include "portmacro.h"
#include "projdefs.h"

namespace freertosxx {

class EventGroup {
 public:
  // Creates a dynamically allocated event group.
  EventGroup();
  virtual ~EventGroup();

  struct WaitOptions {
    bool clear = false;
    bool all = false;
    std::optional<TickType_t> timeout = std::nullopt;
  };
  EventBits_t Wait(EventBits_t bits, WaitOptions opts) {
    return xEventGroupWaitBits(
        handle_,
        bits,
        opts.clear ? pdTRUE : pdFALSE,
        opts.all ? pdTRUE : pdFALSE,
        opts.timeout.value_or(portMAX_DELAY));
  }
  EventBits_t Wait(EventBits_t bits) { return Wait(bits, {}); }

  EventBits_t Set(EventBits_t bits) {
    return xEventGroupSetBits(handle_, bits);
  }

  // Sets the bits from an ISR. Returns whether the bits are set successfully.
  bool SetFromISR(EventBits_t bits, BaseType_t& higher_priority_task_woken) {
    const bool success =
        xEventGroupSetBitsFromISR(handle_, bits, &higher_priority_task_woken);
    if (!success) [[unlikely]] {
      higher_priority_task_woken = pdFALSE;
    }
    return success;
  }

  EventBits_t Clear(EventBits_t bits) {
    return xEventGroupClearBits(handle_, bits);
  }
  bool ClearFromISR(EventBits_t bits) {
    return xEventGroupClearBitsFromISR(handle_, bits) == pdTRUE;
  }

  EventBits_t Get() const { return xEventGroupGetBits(handle_); }
  EventBits_t GetFromISR() const { return xEventGroupGetBitsFromISR(handle_); }

  EventBits_t Sync(
      EventBits_t set, EventBits_t wait_for,
      TickType_t timeout = portMAX_DELAY) {
    return xEventGroupSync(handle_, set, wait_for, timeout);
  }

 protected:
  EventGroup(EventGroupHandle_t handle) : handle_(handle) {}

 private:
  EventGroupHandle_t handle_;
};

class StaticEventGroup : public EventGroup {
 public:
  StaticEventGroup() : EventGroup(xEventGroupCreateStatic(&storage_)) {}
  ~StaticEventGroup() override = default;

 private:
  StaticEventGroup_t storage_;
};

}  // namespace freertosxx

#endif  // FREERTOSXX_EVENT_H