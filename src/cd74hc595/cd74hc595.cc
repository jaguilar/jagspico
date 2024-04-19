#include "jagspico/cd74hc595.h"

#include "cd74hc595.pio.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"
#include "hardware/structs/clocks.h"

namespace jagspico {

Cd74Hc595DriverPio::Cd74Hc595DriverPio(Cd74Hc595DriverPio &&o)
    : pio_(o.pio_), state_machine_(o.state_machine_),
      output_bits_(o.output_bits_) {
  o.pio_ = nullptr;
  o.state_machine_ = 5; // Invalid
}
Cd74Hc595DriverPio::~Cd74Hc595DriverPio() {
  if (!pio_)
    return;
  pio_sm_set_enabled(pio_, state_machine_, false);
  pio_sm_unclaim(pio_, state_machine_);
}

std::optional<Cd74Hc595DriverPio>
Cd74Hc595DriverPio::Create(const Config &config) {
  const PIO pio = config.pio;
  uint32_t sm = pio_claim_unused_sm(pio, true);
  for (uint32_t pin :
       {config.pin_srclk, config.pin_srclk + 1, config.pin_ser}) {
    pio_gpio_init(pio, pin);
  }
  if (!pio_can_add_program(pio, &cd74hc595_program)) {
    panic("can't add cd74hc595 to PIO\n");
  }
  uint32_t offset = pio_add_program(pio, &cd74hc595_program);

  pio_sm_set_consecutive_pindirs(pio, sm, config.pin_srclk, 2, true);
  pio_sm_set_consecutive_pindirs(pio, sm, config.pin_ser, 1, true);
  pio_sm_config sm_config = cd74hc595_program_get_default_config(offset);
  sm_config_set_clkdiv(&sm_config,
                       static_cast<float>(clock_get_hz(clk_sys) * 1.0 /
                                          config.target_frequency));
  sm_config_set_out_pins(&sm_config, config.pin_ser, 1);
  sm_config_set_sideset_pins(&sm_config, config.pin_srclk);

  // We output the MSB first, so we are shifting left. This means that each
  // input must be the highest 8 bits of the 32-bit word.
  sm_config_set_out_shift(&sm_config, false, true, config.output_bits);
  pio_sm_init(pio, sm, offset, &sm_config);
  pio_sm_exec(pio, sm, pio_encode_set(pio_y, config.output_bits - 1));
  pio_sm_set_enabled(pio, sm, true);
  return Cd74Hc595DriverPio(config, sm);
}

} // namespace jagspico