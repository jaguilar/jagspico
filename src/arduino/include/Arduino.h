#ifndef JAGSPICO_ARDUINO_H
#define JAGSPICO_ARDUINO_H

#include <cstdint> // IWYU pragma: export
#include <cstdio>
#include <string>
#include <string_view>

using byte = unsigned char;
class String : public std::string {
public:
  using std::string::string;
  bool equalsIgnoreCase(std::string_view s) const;
};

enum PinMode { INPUT, OUTPUT, INPUT_PULLUP };

void pinMode(int pin, PinMode mode);

enum DigitalWriteLevel { LOW, HIGH };
void digitalWrite(int pin, DigitalWriteLevel value);

enum PrintFormat { DECIMAL, HEX };
class SerialClass {
public:
  static void print(std::string_view s);
  template <typename Integer>
  static void print(Integer n, PrintFormat format = DECIMAL) {
    std::string_view format_str = format == DECIMAL ? "%d" : "%x";
    printf(format_str.data(), n);
  }
  static void println(std::string_view s);
  template <typename Integer>
  static void println(Integer n, PrintFormat format = DECIMAL) {
    std::string_view format_str = format == DECIMAL ? "%d\n" : "%x\n";
    printf(format_str.data(), format);
  }
  static void println();
};
extern SerialClass Serial;

// Delays for milliseconds.
void delay(uint32_t ms);

// Delays for microseconds.
void delayMicroseconds(uint32_t us);

#endif // JAGSPICO_ARDUINO_H