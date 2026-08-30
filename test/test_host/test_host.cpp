#include "minitest.h"
#include "dex.h"
#include "pet.h"
#include "boards/lcd_185c/pins.h"

// pin_config.h cannot be included here: it pulls the panel/touch HAL.
// SX/SY must stay in lockstep with pin_config.h (DESIGN_W 466).
#define DESIGN_W 466
#define SX(x) ((int)((long)(x) * LCD_WIDTH / DESIGN_W))

void test_dex_count() {
  TEST_ASSERT_EQUAL(151, DEX_COUNT);
  TEST_ASSERT_EQUAL_STRING("?", DEX_TBL[0].name);
  TEST_ASSERT_EQUAL_STRING("BULBASAUR", DEX_TBL[1].name);
  TEST_ASSERT_EQUAL_STRING("MEW", DEX_TBL[151].name);
}

void test_charmander_line() {
  TEST_ASSERT_EQUAL(5, DEX_TBL[4].evolvesTo);
  TEST_ASSERT_EQUAL(16, DEX_TBL[4].evolveLevel);
  TEST_ASSERT_EQUAL(R_COMUN, DEX_TBL[4].rarity);
  TEST_ASSERT_EQUAL(6, DEX_TBL[5].evolvesTo);
  TEST_ASSERT_EQUAL(0, DEX_TBL[6].evolvesTo);
  TEST_ASSERT_EQUAL(R_EVO, DEX_TBL[6].rarity);
}

void test_eevee_and_legendaries() {
  TEST_ASSERT_EQUAL(133, DEX_EEVEE);
  TEST_ASSERT_EQUAL(134, DEX_TBL[DEX_EEVEE].evolvesTo);
  TEST_ASSERT_EQUAL(R_LEGENDARIO, DEX_TBL[150].rarity);
  TEST_ASSERT_EQUAL(R_LEGENDARIO, DEX_TBL[151].rarity);
}

void test_evo_targets_in_range() {
  for (int i = 1; i <= DEX_COUNT; i++) {
    uint8_t to = DEX_TBL[i].evolvesTo;
    TEST_ASSERT_LESS_OR_EQUAL(DEX_COUNT, to);
    TEST_ASSERT_NOT_EQUAL(i, to);
    int hops = 0;
    int d = i;
    while (DEX_TBL[d].evolvesTo && hops < 5) {
      d = DEX_TBL[d].evolvesTo;
      hops++;
    }
    TEST_ASSERT_LESS_THAN(5, hops);
  }
}

void test_lcd_185c_scale() {
  TEST_ASSERT_EQUAL(360, LCD_WIDTH);
  TEST_ASSERT_EQUAL(360, LCD_HEIGHT);
  TEST_ASSERT_EQUAL(4, PET_SCALE);
  TEST_ASSERT_EQUAL(360, SX(466));
  TEST_ASSERT_EQUAL(0, SX(0));
  TEST_ASSERT_EQUAL((int)((long)78 * 360 / 466), SX(78));
}

void test_pet_inline_rules() {
  Pet p;
  TEST_ASSERT_TRUE(p.isEgg());
  p.speciesId = 4;
  TEST_ASSERT_FALSE(p.isEgg());
  TEST_ASSERT_TRUE(p.lovesBerry(4 % 3));
  TEST_ASSERT_FALSE(p.lovesBerry(0));

  p.ageMinutes = 0;
  TEST_ASSERT_EQUAL(1, p.level());
  p.ageMinutes = 60;
  TEST_ASSERT_EQUAL(2, p.level());
  p.ageMinutes = 59;
  TEST_ASSERT_EQUAL(1, p.level());

  p.fullness = 10;
  p.joy = 40;
  p.energy = 20;
  p.hygiene = 30;
  TEST_ASSERT_EQUAL(10, p.lowestStat());

  TEST_ASSERT_FALSE(p.isRegistered(4));
  p.dexReg[0] = 1 << 3;  // dex 4
  TEST_ASSERT_TRUE(p.isRegistered(4));
  TEST_ASSERT_FALSE(p.isRegistered(0));
  TEST_ASSERT_FALSE(p.isRegistered(152));
}

void test_i18n_ids() {
  TEST_ASSERT_EQUAL(6, LANG_COUNT);
  TEST_ASSERT_EQUAL(LANG_EN, LANG_DEFAULT);
  TEST_ASSERT_TRUE(STR_COUNT > 0);
}

int main() {
  RUN_TEST(test_dex_count);
  RUN_TEST(test_charmander_line);
  RUN_TEST(test_eevee_and_legendaries);
  RUN_TEST(test_evo_targets_in_range);
  RUN_TEST(test_lcd_185c_scale);
  RUN_TEST(test_pet_inline_rules);
  RUN_TEST(test_i18n_ids);
  return mt_end();
}
