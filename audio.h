#pragma once
#include <stdint.h>

// Game sound effects (queued, non-blocking). Order matches the SFX table in
// audio.cpp.
enum Sfx : uint8_t {
  SFX_TAP = 0,  // tap / button
  SFX_EAT,      // eat
  SFX_PLAY,     // minigame point / hit
  SFX_HEART,    // like / pet
  SFX_HATCH,    // hatch
  SFX_EVOLVE,   // evolve
  SFX_MEDAL,    // medal / milestone
  SFX_DENY,     // action not allowed
  SFX_BYE,      // farewell
  SFX_LEVEL,    // level up
  SFX_COUNT
};

void audioBegin();          // init ES8311 + I2S + amp + audio task
void sfxPlay(uint8_t id);   // enqueue an effect (does not block the loop)
void audioSetEnabled(bool on);
bool audioEnabled();
