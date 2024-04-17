#ifndef CD74HC595_H
#define CD74HC595_H

#include <cstdint>
#include <optional>

#include "hardware/pio.h"

namespace jagspico {

class Cd74Hc595DriverPio {
public:
  struct Config {
    PIO pio;
    int pin_srclk = -1; // Note: pin_rclk must be pin_srclk + 1.
    int pin_ser = -1;
    uint32_t target_frequency = 1'000'000;

    // Number of bits to set before ticking rclk, loading those bits into
    // the storage register of the Cd74Hc595. Currently this driver supports
    // at most 32 bits.
    //
    // This is useful if you have more than one Cd74Hc595 connected in
    // series. The highest bit will be the one first shifted out (i.e. it
    // will be q7 or qh on the last chip in the chain.).
    int output_bits = 8;
  };

  Cd74Hc595DriverPio(const Cd74Hc595DriverPio &) = delete;
  Cd74Hc595DriverPio(Cd74Hc595DriverPio &&o);
  Cd74Hc595DriverPio &operator=(const Cd74Hc595DriverPio &) = delete;

  ~Cd74Hc595DriverPio();

  // Constructs the driver from the Config.
  static std::optional<Cd74Hc595DriverPio> Create(const Config &config);

  inline void Send(uint32_t x) {
    pio_sm_put_blocking(pio_, state_machine_, x << (32 - output_bits_));
  }

  uint32_t state_machine() const { return state_machine_; }

private:
  Cd74Hc595DriverPio(const Config &config, uint32_t state_machine)
      : pio_{config.pio}, state_machine_{state_machine},
        output_bits_(config.output_bits) {}

  PIO pio_;
  uint32_t state_machine_;
  int output_bits_;
};

} // namespace jagspico

#endif // CD74HC595_H