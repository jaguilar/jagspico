#ifndef JAGSPICO_DISP4DIGIT_DISP4DIGIT_H
#define JAGSPICO_DISP4DIGIT_DISP4DIGIT_H

#include <array>
#include <cstdint>
#include <functional>
#include <utility>

#include "FreeRTOS.h"
#include "event_groups.h"
#include "jagspico/cd74hc595.h"
#include "queue.h"
#include "task.h"

namespace jagspico {

class Disp4Digit {
 public:
  // Contains the value to display. digits must be 0-9, MSB first. Decimal
  // position is the digit after which the decimal is positioned (MSB being 3,
  // LSB being 0).
  struct DisplayValue {
    uint16_t digits;
    uint8_t decimal_position;
    bool off = false;  // If true, turn off all segments.

    static DisplayValue FromFloat(float f);
    static DisplayValue FromInt(int i);
  };

  struct Config {
    Cd74Hc595DriverPio digit_driver;

    // The select pin for the most significant digit.
    // The less significant digits will be the 3 following pins.
    uint32_t pin_select = 12;

    // Called each time the display refreshes. Avoid blocking
    // in this function to keep the display responsive.
    std::move_only_function<DisplayValue()> get_content_callback;
  };

  Disp4Digit(Config&& config);
  ~Disp4Digit();
  Disp4Digit(const Disp4Digit&) = delete;
  Disp4Digit& operator=(const Disp4Digit&) = delete;

 private:
  // Converts a digit to a mask for display on the 4-digit display.
  static uint8_t DigitToMask(uint8_t digit, bool decimal_after);

  // Task to drive the display with values set in digits_mask_.
  void DriveTask();

  Cd74Hc595DriverPio digit_driver_;
  const uint32_t pin_select_;
  std::move_only_function<DisplayValue()> callback_;

  EventGroupHandle_t shutdown_event_ = xEventGroupCreate();
};

}  // namespace jagspico

#endif  // JAGSPICO_DISP4DIGIT_DISP4DIGIT_H