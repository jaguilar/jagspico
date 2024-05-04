#ifndef JAGSPICO_ARDUINO_EEPROM_H
#define JAGSPICO_ARDUINO_EEPROM_H

#include <Arduino.h>

class EEPROMClass {
public:
  static void get(uint32_t address, uint16_t &value);
  static void put(uint32_t address, uint16_t value);
};
EEPROMClass EEPROM;

#endif // JAGSPICO_ARDUINO_EEPROM_H