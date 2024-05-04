#include <Arduino.h>

#include "FreeRTOS.h"
#include "hardware/gpio.h"
#include "task.h"

void pinMode(int pin, PinMode mode) {
  gpio_init(pin);
  gpio_set_dir(pin, mode == OUTPUT);
  if (mode == INPUT_PULLUP) {
    gpio_pull_up(pin);
  }
}

void digitalWrite(int pin, DigitalWriteLevel value) {
  gpio_put(pin, value == HIGH);
}

void SerialClass::print(std::string_view s) {
  printf("%*s", s.size(), s.data());
}

void SerialClass::println(std::string_view s) {
  printf("%*s\n", s.size(), s.data());
}

void SerialClass::println() { printf("\n"); }

bool String::equalsIgnoreCase(std::string_view s) const {
  return size() == s.size() &&
         std::equal(begin(), end(), s.begin(), s.end(),
                    [](char a, char b) { return tolower(a) == tolower(b); });
}

void delay(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
