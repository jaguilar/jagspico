#include "disp4digit.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <utility>

#include "hardware/gpio.h"
#include "portmacro.h"
#include "projdefs.h"
#include "task.h"

namespace jagspico {

static uint8_t DigitToMask(uint8_t digit, bool decimal_after) {
  if (digit > 9) {
    panic("DigitToMask: OOB %d\n", digit);
  }
  constexpr uint8_t dot_mask = 0b10000000;
  constexpr std::array<uint8_t, 10> digits = {
      0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110,
      0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111,
  };
  return digits.at(digit) | (decimal_after ? dot_mask : 0U);
}

static uint32_t DigitToMask4(std::array<uint8_t, 4> values,
                             int decimal_position) {
  uint32_t out = 0;
  for (int i = 0; i < 4; ++i) {
    const int digit_index = 3 - i;  // Note: LSB first.
    const int shl = 24 - i * 8;
    const uint32_t digit_mask =
        DigitToMask(values[i], decimal_position == (3 - i));
    out |= (digit_mask << shl);
  }
  return out;
}

Disp4Digit::Disp4Digit(Config&& config)
    : digit_driver_(std::move(config.digit_driver)),
      pin_select_(config.pin_select),
      callback_(std::move(config.get_content_callback)) {
  for (int i = 0; i < 4; ++i) {
    const uint32_t p = pin_select_ + i;
    gpio_init(p);
    gpio_set_dir(p, GPIO_OUT);
    // Drive the GPIO to VCC, preventing current from flowing.
    gpio_put(p, true);
  }

  auto task_ok = xTaskCreate(
      +[](void* obj) { static_cast<Disp4Digit*>(obj)->DriveTask(); },
      "disp4digit", 256, this, 1, nullptr);
  assert(task_ok == pdPASS);
}

constexpr EventBits_t kStartShutdown = 0b1;
constexpr EventBits_t kShutdownFinished = 0b10;

Disp4Digit::~Disp4Digit() {
  xEventGroupSetBits(shutdown_event_, kStartShutdown);
  xEventGroupWaitBits(shutdown_event_, kShutdownFinished, pdTRUE, pdTRUE,
                      portMAX_DELAY);
  vEventGroupDelete(shutdown_event_);
}

void Disp4Digit::DriveTask() {
  uint32_t prev_pin = pin_select_;
  while (true) {
    if (xEventGroupGetBits(shutdown_event_) == kStartShutdown) {
      xEventGroupSetBits(shutdown_event_, kShutdownFinished);
    }

    const DisplayValue value = callback_();
    const std::array<uint8_t, 4> digits = {
        static_cast<uint8_t>(value.digits / 1000 % 10),
        static_cast<uint8_t>(value.digits / 100 % 10),
        static_cast<uint8_t>(value.digits / 10 % 10),
        static_cast<uint8_t>(value.digits % 10)};
    const uint32_t digits_mask_ = DigitToMask4(digits, value.decimal_position);
    for (int i = 0; i < 4; ++i) {
      gpio_put(prev_pin, true);  // Unselect the previous selection pin.
      const uint32_t pin = pin_select_ + i;
      gpio_put(
          pin,
          false);  // Drive the selected pin low to begin receiving current.
      prev_pin = pin;
      digit_driver_.Send(digits_mask_ >> ((3 - i) * 8) & 0xff);
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
}

}  // namespace jagspico