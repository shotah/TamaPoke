#pragma once
#include <Arduino.h>

// Supported languages. Latin UI is the built-in 6x8 face (no accents).
// JA / ZH use Unifont from /mons/font_ja.bin and font_zh.bin on the SD.
enum Lang : uint8_t {
  LANG_ES = 0, LANG_EN, LANG_FR, LANG_DE, LANG_IT, LANG_PT,
  LANG_JA, LANG_ZH, LANG_COUNT
};
#define LANG_DEFAULT LANG_EN  // default language: English

extern Lang gLang;  // active language (defined in i18n.cpp)

// String IDs. Order must match the STRINGS table in i18n.cpp.
enum StrId : uint8_t {
  // pet status (statusMsg)
  S_EVOLVING, S_EATING, S_LIKES, S_HUNGRY, S_NEEDS_BATH,
  S_EXHAUSTED, S_SAD, S_CHUBBY, S_IS_SHINY, S_HAPPY,
  // farewell ceremonies
  S_FAREWELL, S_RUNAWAY, S_GOODBYE,
  // egg
  S_EGG_HDR, S_EGG_LEGEND, S_EGG_RARE, S_EGG_TOUCH, S_EGG_MOVES, S_EGG_ALMOST,
  // shared formats
  S_POKEDEX_FMT,   // "POKEDEX %u/151"
  S_NAME_FMT,      // "%s%s Nv.%u"
  // release dialog
  S_RELEASE_FMT, S_YES, S_NO,
  // minigame and punching bag
  S_HITS_FMT, S_STR_GAIN_FMT, S_NEW_RECORD, S_RECORD_FMT, S_HIT_FAST,
  S_SCORE_FMT, S_GREAT_JOY, S_PLUS_JOY,
  // clock / settings
  S_SET_TIME, S_HOUR, S_MIN, S_CLOCK_CANCEL, S_LANG_LABEL,
  // celebration
  S_MEDAL_BANNER, S_GREAT, S_STREAK_DAYS_FMT,
  // sheet: profile
  S_STREAK_FMT, S_VIN, S_BERRY_UNK, S_BERRY_RED, S_BERRY_BLUE, S_BERRY_GREEN,
  S_INFO_FMT, S_RENAME_HINT,
  // sheet: battle
  S_BATTLE, S_STAT_ATK, S_STAT_DEF, S_STAT_SPE, S_STAT_WGT, S_TRAIN_STR,
  // sheet: medals
  S_MEDALS_FMT, S_BACK,
  // keyboard and gallery
  S_NAME, S_DETAIL_BACK,
  // bars
  S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE, S_BAR_HYG,
  // live minigame scoreboard
  S_REC_FMT,
  // sheet: progress page
  S_PROGRESS, S_LVL_FMT, S_NEXT_LVL_FMT, S_EVO_LABEL, S_FINAL_FORM,
  S_EVO_READY, S_EVO_BLOCKED, S_EVO_IN_FMT, S_MISTAKES_FMT,
  // sound toggle (settings)
  S_SND_ON, S_SND_OFF,
  S_EVO_TAP,        // evolve button text
  S_FAREWELL_BTN,   // farewell button text (includes the name: "%s ...")
  S_RUNAWAY_BTN,    // runaway-from-neglect button text (sad ending)
  // decision dialogs (evolve/keep, farewell/stay)
  S_EVO_Q, S_EVO_KEEP, S_FAR_Q, S_FAR_GO, S_FAR_STAY,
  S_CHOOSE_STARTER,  // starter choice title (first time)
  S_NO_SPRITES, S_LOAD_SPRITES,  // warning when the sprite is missing from SD
  S_DEX_HINT,        // unregistered dex detail
  S_EGG_BLESS,       // egg screen: farewell helps the next roll
  S_HOWTO_1, S_HOWTO_2,  // one-time card after starter
  S_EXIT,                // minigame quit (top-center; round screen has no corners)
  S_SICK,                // dirty nap; medicine in the food tray
  S_DEF_GAIN_FMT,        // brace result: "DEF +%u"
  STR_COUNT
};

const char *T(StrId id);       // text in the active language
const char *medalName(int i);  // medal banner (MED_COUNT)
const char *medalLabel(int i); // short medal label
const char *medalDesc(int i);  // long medal description

void loadLang();             // read language from NVS (call in setup)
void setLang(Lang l);        // change and persist the language
