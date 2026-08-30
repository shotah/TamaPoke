#pragma once
#include <stdint.h>

// Host stub. Pet methods that hit NVS are not compiled into native tests.

class Preferences {
public:
  bool begin(const char *, bool = false) { return true; }
  void end() {}
  void clear() {}
  uint8_t getUChar(const char *, uint8_t def = 0) { return def; }
  void putUChar(const char *, uint8_t) {}
};
