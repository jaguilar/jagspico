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
    const int shl = 24 - i * 8;
    const uint32_t digit_mask = DigitToMask(values[i], decimal_position == i);
    out |= (digit_mask << shl);
  }  //
  return out;
}

Disp4Digit::Disp4Digit(Config&& config)
    : digit_driver_(std::move(config.digit_driver)),
      pin_select_(config.pin_select) {
  for (int i = 0; i < 4; ++i) {
    const uint32_t p = pin_select_ + i;
    gpio_init(p);
    gpio_set_dir(p, GPIO_OUT);
    // Drive the GPIO to VCC, preventing current from flowing.
    gpio_put(p, true);
  }

  command_queue_ = xQueueCreate(1, sizeof(QueueCommand));
  digits_mask_ = DigitToMask4({0, 0, 0, 0}, -1);
  auto task_ok = xTaskCreate(
      +[](void* obj) { static_cast<Disp4Digit*>(obj)->DriveTask(); },
      "disp4digit", 256, this, 1, nullptr);
  assert(task_ok == pdPASS);
}

Disp4Digit::~Disp4Digit() {
  QueueCommand cmd{.shut_down = true};
  xQueueSend(command_queue_, &cmd, portMAX_DELAY);
  auto result = xQueueReceive(command_queue_, &cmd, portMAX_DELAY);
  assert(result == pdTRUE);

  // Safe to delete everything else.
  vQueueDelete(command_queue_);
}

void Disp4Digit::Set(std::array<uint8_t, 4> digits, int decimal_position) {
  QueueCommand cmd{.set_mask = DigitToMask4(digits, decimal_position)};
  xQueueSend(command_queue_, &cmd, portMAX_DELAY);
}

void Disp4Digit::DriveTask() {
  uint32_t prev_pin = pin_select_;
  while (true) {
    for (int i = 0; i < 4; ++i) {
      QueueCommand cmd;
      // We wait up to 5ms between selecting new digits.
      if (xQueueReceive(command_queue_, &cmd, pdMS_TO_TICKS(2)) == pdTRUE) {
        if (cmd.shut_down) {
          // Pong the command back. We are promising not to touch *this sending
          // this command.
          xQueueSend(command_queue_, &cmd, portMAX_DELAY);
          vTaskDelete(nullptr);
          return;
        }
        digits_mask_ = cmd.set_mask;
        break;  // Restart the process of printing a new number.
      }

      gpio_put(prev_pin, true);  // Unselect the previous selection pin.
      const uint32_t pin = pin_select_ + i;
      gpio_put(
          pin,
          false);  // Drive the selected pin low to begin receiving current.
      prev_pin = pin;
      digit_driver_.Send(digits_mask_ >> ((3 - i) * 8) & 0xff);
    }
  }
}

}  // namespace jagspico