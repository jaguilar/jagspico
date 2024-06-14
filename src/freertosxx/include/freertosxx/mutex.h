#ifndef JAGSPICO_UTIL_MUTEX_H
#define JAGSPICO_UTIL_MUTEX_H

#include <FreeRTOSConfig.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

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
  Mutex& operator=(const Mutex&) = delete;

  // Note: a moved-from mutex cannot be used.
  Mutex(Mutex&& o);
  Mutex& operator=(Mutex&& o);

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

  bool LockIsHeldByCurrentTask() const {
    return xSemaphoreGetMutexHolder(mutex_) == xTaskGetCurrentTaskHandle();
  }

 private:
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

template <typename T>
class Borrowable;

// This type contains a resource and a mutex that protects it. The mutex is
// already locked, and destroying the instance of this class will unlock it.
// Therefore, while this class exists, the underlying resource can be safely
// accessed from the current thread.
template <typename T>
class BorrowedPointer {
 public:
  BorrowedPointer(T* value, Mutex* mutex) : value_(value), mutex_(mutex) {
    configASSERT(mutex_->LockIsHeldByCurrentTask());
  }

  template <typename U>
  BorrowedPointer(BorrowedPointer<U>&& other) {
    *this = std::forward<BorrowedPointer<U&&>>(other);
  }

  template <typename U>
  BorrowedPointer<T>& operator==(BorrowedPointer<U>&& o) {
    static_assert(std::is_convertible_v<U*, T*>);
    if (mutex_ != nullptr) mutex_->Unlock();
    value_ = std::exchange(o.value_, nullptr);
    mutex_ = std::exchange(o.mutex_, nullptr);
  }

  BorrowedPointer(const BorrowedPointer<T>&) = delete;
  BorrowedPointer& operator=(const BorrowedPointer<T>&) = delete;

  ~BorrowedPointer() {
    if (mutex_ != nullptr) mutex_->Unlock();
  }

  T* operator->() {
    configASSERT(mutex_ != nullptr);
    return value_;
  }
  T& operator*() {
    configASSERT(mutex_ != nullptr);
    return *value_;
  }

  operator bool() const { return mutex_ != nullptr; }

  void release() { *this = {nullptr, nullptr}; }

  template <typename U>
  operator Borrowable<U>() {
    static_assert(std::is_convertible_v<T*, U*>);
    return Borrowable<U>(value_, mutex_);
  }

 private:
  T* value_ = nullptr;
  Mutex* mutex_ = nullptr;
};

// Significantly inspired by Pigweed, but probably a little less flexible.
// For example, we just use the above Mutex class. We'll probably migrate
// someday to Pigweed if they get the Pico W to be fully supported.
//
// This is a pointer-like type and should be passed by value. It's a thing
// that can be borrowed. The underlying object and mutex must outlive this.
template <typename T>
class Borrowable {
 public:
  Borrowable(T* value, Mutex* mutex) : value_(value), mutex_(mutex) {}
  template <typename U>
  Borrowable(U* value, Mutex* mutex) : value_(value), mutex_(mutex) {
    static_assert(std::is_convertible_v<U*, T*>);
  }

  Borrowable(Borrowable<T>&& o) = default;
  Borrowable& operator=(Borrowable<T>&& o) = default;
  Borrowable(const Borrowable<T>& o) = default;
  Borrowable& operator=(const Borrowable<T>& o) = default;

  // Borrows the resource. Waits until the resource is available.
  BorrowedPointer<T> Borrow() {
    mutex_->Lock();
    return BorrowedPointer<T>(value_, mutex_);
  }

  // Tries to borrow the resource. If it can't be borrowed by the time
  // ms passes, returns nullopt.
  std::optional<BorrowedPointer<T>> TryBorrow(int ms) {
    if (!mutex_->LockWithTimeout(ms)) {
      return std::nullopt;
    }
    return BorrowedPointer<T>(value_, mutex_);
  }

 private:
  T* value_;
  Mutex* mutex_;
};

// A Borrowable object that owns its own mutex.
template <class T>
class IntrusiveBorrowable : public Borrowable<T> {
 public:
  IntrusiveBorrowable() : Borrowable<T>(this, &mutex_) {}

 private:
  Mutex mutex_;
};

// A borrowable that owns the resource.
template <typename T>
class OwnerBorrowable {
 public:
  template <typename... Args>
  OwnerBorrowable(std::in_place_t, Args&&... args) {
    value_ = std::make_unique<T>(std::forward<Args>(args)...);
  }
  explicit OwnerBorrowable(std::unique_ptr<T> value)
      : value_(std::move(value)) {}
  explicit OwnerBorrowable(T* value) : value_(value) {}

  OwnerBorrowable(OwnerBorrowable<T>&& o) = default;
  OwnerBorrowable& operator=(OwnerBorrowable<T>&& o) = default;

  operator Borrowable<T>() { return Borrowable<T>(&value_, &mutex_); }

  // Borrows the resource. Waits until the resource is available.
  BorrowedPointer<T> Borrow() {
    mutex_.Lock();
    return BorrowedPointer<T>(&value_, &mutex_);
  }

  // Tries to borrow the resource. If it can't be borrowed by the time
  // ms passes, returns nullopt.
  std::optional<BorrowedPointer<T>> TryBorrow(int ms) {
    if (!mutex_.LockWithTimeout(ms)) {
      return std::nullopt;
    }
    return BorrowedPointer<T>(&value_, &mutex_);
  }

 private:
  T value_;
  Mutex mutex_;
};

}  // namespace freertosxx

#endif  // JAGSPICO_UTIL_MUTEX_H