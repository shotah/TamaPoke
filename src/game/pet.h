#pragma once
#include <Arduino.h>
#include <Preferences.h>

// 1 tick = 1 minute of play. Lower this to test faster
// (e.g. 5000UL = stats drop 12x faster).
#define PET_TICK_MS 60000UL
#define SLEEP_ENERGY 10  // ENE per minute asleep (live and offline)
#define SLEEP_HYG_FLOOR 20  // HYG can fall far enough to get sick
#define SICK_HYG 35         // asleep + HYG below this → sick
// Minutes of play per level. At 60, CHARMANDER evolves after ~16 h
// of play with perfect care. Lower to 1 to see evolutions immediately.
#define MINUTES_PER_LEVEL 60
#define EAT_ANIM_MS 2500UL
#define HEART_MS 1500UL
#define EVOLVE_ANIM_MS 5200UL              // evolution animation (longer = more epic)
#define CEREMONY_MS 10000UL                // on-screen farewell duration
#define FAREWELL_AGE_MIN (3UL * 24 * 60)   // farewell after 3 days of play (final form)
#define RUNAWAY_TICKS 60                   // runs away after 1 h with ALL stats at zero

// end-of-cycle ceremonies
enum : uint8_t { CER_NONE = 0, CER_FAREWELL, CER_RUNAWAY, CER_RELEASE };

enum PetMood : uint8_t { MOOD_HAPPY, MOOD_SAD, MOOD_EATING, MOOD_SLEEPING };

// individual medals (bitmask)
enum : uint16_t {
  MED_LV10 = 1 << 0, MED_LV25 = 1 << 1, MED_LV50 = 1 << 2,
  MED_BERRY = 1 << 3, MED_STREAK7 = 1 << 4, MED_BOND = 1 << 5,
  MED_FINAL = 1 << 6, MED_FIT = 1 << 7,
};
#define MED_COUNT 8

class Pet {
public:
  // Stats 0..100
  uint8_t fullness = 80;  // food
  uint8_t joy = 80;       // happiness
  uint8_t energy = 80;    // energy
  uint8_t hygiene = 100;  // cleanliness
  uint8_t poops = 0;      // poops on screen (max 3)
  uint8_t weight = 0;     // 0-100: candy fattens, minigame burns it off
  // genes (90-110%, rolled at hatch) and training (0-100)
  uint8_t geneAtk = 100, geneDef = 100, geneSpe = 100;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  bool berryKnown = false;  // already discovered its favorite berry
  bool shiny = false;       // rare color variant (rolled in the egg)
  uint32_t ageMinutes = 0;
  int16_t speciesId = -1;      // Pokedex number (1-151), -1 = egg
  int16_t prevSpeciesId = -1;  // for the evolution animation
  uint8_t careMistakes = 0;   // neglect: each delays evolution by 1 level
  bool sleeping = false;
  bool sick = false;      // dirty nap: Hurt until medicine
  uint32_t lastSeenEpoch = 0;   // last RTC time seen (for offline progression)
  uint8_t ceremony = CER_NONE;  // farewell/runaway/release in progress
  uint8_t lastEnd = CER_NONE;   // how the previous one ended (affects the egg)
  uint8_t dexReg[19] = { 0 };       // raised-species pokedex (151-bit bitmap)
  uint8_t dexShinyReg[19] = { 0 };  // raised in shiny form
  // daily care streak (player: persists across raisings)
  uint16_t streak = 0, bestStreak = 0;
  uint32_t lastCareDay = 0;
  // bond (of the pet: rises slowly with care, resets when another hatches)
  uint8_t bond = 0;
  char nick[12] = "";    // nickname (empty = species name)
  // medals: of the individual + cumulative count across all raisings
  uint16_t medals = 0, totalMedals = 0;
  uint16_t newMedal = 0;   // newly earned, for celebration
  uint16_t lastMilestone = 0;  // streak milestone already celebrated
  uint16_t gameHi = 0;     // minigame high score (player)
  uint16_t strHi = 0;      // punching-bag hit record
  uint16_t walkHi = 0;     // walk-runner distance record
  uint16_t braceHi = 0;    // brace-block record

  void begin();                 // load state from NVS (or create the first egg)
  void update(uint32_t nowMs);  // call every loop()

  // Actions (touch buttons)
  void feed();              // red berry (compat)
  void feedBerry(uint8_t color);  // 0 red, 1 blue, 2 green
  void feedCandy();
  void giveMedicine();    // cures sick; deny no-ops if healthy
  bool lovesBerry(uint8_t color) const {
    return !isEgg() && (speciesId % 3) == color;  // hidden taste by species
  }
  void playResult(uint8_t score);  // minigame reward (trains SPE)
  uint8_t trainStrength(uint16_t hits);  // punching bag (trains ATK)
  void walkResult(uint16_t dist);  // walk runner (trains SPE, burns weight)
  uint8_t trainDefense(uint16_t blocks);  // brace (trains DEF)

  // combat stats: real gen 1 base x genes + level + training
  uint16_t atkStat() const;
  uint16_t defStat() const;
  uint16_t speStat() const;
  void play();
  void toggleLight();  // sleep / wake
  void clean();
  void caress();  // pet the creature
  void eggTap();  // tap the egg: 3 taps and it hatches
  void newEgg();   // start over with a random starter
  void release();  // release (long press + confirm)
  void syncClock(uint32_t nowEpoch);  // apply time that passed while powered off
  void setClock(uint32_t nowEpoch);   // set the clock without applying progression
  void startFarewell();  // also usable from the serial console (BYE)
  void startRunaway();   // also usable from the serial console (RUN)

  bool isEgg() const { return speciesId < 0; }
  uint8_t eggCracks() const { return eggTaps; }
  bool eating() const { return millis() < eatUntil; }
  bool showHeart() const { return millis() < heartUntil; }
  bool evolving() const { return millis() < evolveUntil; }
  float evolveT() const {     // evolution animation progress 0..1
    uint32_t n = millis();
    uint32_t left = evolveUntil > n ? evolveUntil - n : 0;
    return 1.0f - (float)left / (float)EVOLVE_ANIM_MS;
  }
  bool canEvolveNow() const;  // evolution conditions met (ready)
  void evolve();              // trigger the transformation (called by a user tap)
  bool canFarewellNow() const;  // final form + 7 days: ready to farewell (button)
  bool canRunawayNow() const;   // total neglect 1h: ready to run away (sad button)
  // user decides in a dialog; "keep/stay" postpones and re-offers later
  bool wantEvolveButton() const { return canEvolveNow() && level() > evoDeclinedLv; }
  bool wantFarewellButton() const { return canFarewellNow() && ageMinutes >= farDeclinedAge; }
  void declineEvolve() { evoDeclinedLv = level(); }              // re-offers on level up
  void declineFarewell() { farDeclinedAge = ageMinutes + 1440; } // re-offers in 1 day
  // first play: player chooses starter (Bulbasaur/Charmander/Squirtle)
  bool awaitingStarter() const { return starterPick; }
  void chooseStarter(int16_t dex);
  bool showHowto() const { return !howtoSeen && !starterPick; }
  void dismissHowto();
  void factoryReset() { prefs.clear(); }  // clears NVS (test: serial command WIPE)
  void dbgRunawayReady() { fullness = joy = energy = hygiene = 0; neglectTicks = RUNAWAY_TICKS; }  // test
  void dbgSick() { sick = true; hygiene = 20; }
  uint8_t level() const { return 1 + ageMinutes / MINUTES_PER_LEVEL; }
  bool isRegistered(int16_t dex) const {
    return dex >= 1 && dex <= 151 && (dexReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  bool isShinyRegistered(int16_t dex) const {
    return dex >= 1 && dex <= 151 && (dexShinyReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  uint16_t registeredCount() const;
  bool lineHasUnregistered(int16_t base) const;
  uint8_t eggRarity() const;       // current egg rarity (without revealing species)
  int16_t pickEggSpecies();        // public so rolls can be simulated (EGGS)
  uint8_t lowestStat() const { return min(min(fullness, joy), min(energy, hygiene)); }
  PetMood mood() const;
  // farewell/runaway ceremony progress, 0..1 (for animating it)
  float ceremonyT() const {
    if (ceremony == CER_NONE) return 0.0f;
    uint32_t n = millis();
    uint32_t left = ceremonyUntil > n ? ceremonyUntil - n : 0;
    return 1.0f - (float)left / (float)CEREMONY_MS;
  }

  // streak / bond / medals / name
  void rename(const char *name);
  bool hasMedal(uint16_t m) const { return medals & m; }
  bool showMedal() const { return millis() < medalUntil; }
  bool showMilestone() const { return millis() < milestoneUntil; }
  int careBonus() const;  // egg boost from streak + bond

  // deferred periodic save: tick() marks pending and the loop flushes
  // when the screen is dimmed/off (flash writes freeze both cores
  // ~1s: this way it is not visible and does not cut touch)
  bool savePending() const { return pendingSave; }
  void flushSave();

private:
  Preferences prefs;
  uint32_t lastTick = 0;
  uint32_t eatUntil = 0;
  uint32_t heartUntil = 0;
  uint32_t evolveUntil = 0;
  int16_t eggTarget = 1;       // hidden dex that will come out of the egg
  bool eggShiny = false;       // surprise rolled when creating the egg
  uint8_t eggTaps = 0;
  uint8_t mistakeCooldown = 0;
  uint8_t ticksSinceSave = 0;
  bool pendingSave = false;     // periodic save pending flush
  uint8_t evoDeclinedLv = 0;    // "keep form": do not offer evolution until level up
  uint32_t farDeclinedAge = 0;  // "stay together": do not offer farewell until this age
  bool starterPick = false;     // first play: waiting for the player to choose a starter
  bool howtoSeen = true;        // one-time loop card; default true so old saves skip it
  uint8_t neglectTicks = 0;
  uint16_t goodTicks = 0;  // well-cared streak: forges DEF
  uint32_t ceremonyUntil = 0;
  uint8_t bondToday = 0;       // daily cap on bond gain
  uint32_t medalUntil = 0;     // on-screen medal celebration
  uint32_t milestoneUntil = 0; // streak milestone celebration

  uint32_t today() const { return lastSeenEpoch ? lastSeenEpoch / 86400 : 0; }
  void registerCare();   // first care of the day: streak + bond
  void addBond(uint8_t amt);
  void checkMedals();
  void tick();
  void hatch();
  void registerSpecies(int16_t dex);
  void save();
  void load();
  static uint8_t clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
};
