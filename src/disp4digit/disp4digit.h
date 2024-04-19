#ifndef JAGSPICO_DISP4DIGIT_DISP4DIGIT_H
#define JAGSPICO_DISP4DIGIT_DISP4DIGIT_H

#include <array>
#include <cstdint>
#include <utility>

#include "FreeRTOS.h"
#include "cd74hc595/cd74hc595.h"
#include "queue.h"
#include "task.h"

namespace jagspico {

class Disp4Digit {
 public:
  struct Config {
    Cd74Hc595DriverPio digit_driver;

    // The select pin for the most significant digit.
    // The less significant digits will be the 3 following pins.
    uint32_t pin_select = 12;
  };

  Disp4Digit(Config&& config);
  ~Disp4Digit();
  Disp4Digit(const Disp4Digit&) = delete;
  Disp4Digit& operator=(const Disp4Digit&) = delete;

  // Sets the value on the display. digits must be 0-9. Decimal position
  // is the digit after which the decimal is positioned.
  void Set(std::array<uint8_t, 4> digits, int decimal_position);

 private:
  // Converts a digit to a mask for display on the 4-digit display.
  static uint8_t DigitToMask(uint8_t digit, bool decimal_after);

  // Task to drive the display with values set in digits_mask_.
  void DriveTask();

  Cd74Hc595DriverPio digit_driver_;
  uint32_t pin_select_;

  struct QueueCommand {
    bool shut_down = false;
    uint32_t set_mask = 0;
  };

  // Receives a QueueCommand every time someone calls set, or when it is
  // time to exit.
  QueueHandle_t command_queue_;
  uint32_t digits_mask_;
};

}  // namespace jagspico

#endif  // JAGSPICO_DISP4DIGIT_DISP4DIGIT_H