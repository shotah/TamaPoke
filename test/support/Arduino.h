#pragma once
#include <stdint.h>
#include <stddef.h>

// Enough of Arduino for host tests that include pet.h / i18n.h / dex.h.
// Firmware builds use the real core headers, not this file.

using boolean = bool;

#ifndef min
template <typename T>
const T &min(const T &a, const T &b) {
  return a < b ? a : b;
}
#endif

inline unsigned long millis() { return 0; }
