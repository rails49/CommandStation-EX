// Native test of the rule setTrackMode() uses to tell a modifier-only change
// from a change of Track Mode that needs the full track setup. Run with
// rails49/test-guard.sh. Words as in CONTEXT.md.

#include "../TrackModeChange.h"
#include <cstdio>

static int failures = 0;
static const char *current = "";

#define EXPECT(cond)                                                      \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::printf("FAIL %s\n  %s:%d: %s\n", current, __FILE__, __LINE__,  \
                  #cond);                                                 \
      failures++;                                                         \
    }                                                                     \
  } while (0)

// The mode words as MotorDriver.h has them.
enum : uint8_t {
  NONE = 1, MAIN = 2, PROG = 4, DC = 8, EXT = 16, BOOST = 32,
  INV = 64, AUTO = 128,
};

static bool keeps(uint8_t oldMode, uint8_t mode) {
  return isModifierOnlyChange(oldMode, mode, 0, 0);
}

static void inv_on_is_modifier_only() {
  EXPECT(keeps(MAIN, MAIN | INV));
  EXPECT(keeps(MAIN | AUTO, MAIN | AUTO | INV));
}

static void inv_off_is_modifier_only() {
  EXPECT(keeps(MAIN | INV, MAIN));
  EXPECT(keeps(MAIN | AUTO | INV, MAIN | AUTO));
}

static void auto_on_is_modifier_only() {
  EXPECT(keeps(MAIN, MAIN | AUTO));
  EXPECT(keeps(MAIN | INV, MAIN | AUTO | INV));
}

static void auto_off_is_modifier_only() {
  EXPECT(keeps(MAIN | AUTO, MAIN));
  EXPECT(keeps(MAIN | AUTO | INV, MAIN | INV));
}

static void main_auto_and_main_auto_inv_swap_without_setup() {
  EXPECT(keeps(MAIN | AUTO, MAIN | AUTO | INV));
  EXPECT(keeps(MAIN | AUTO | INV, MAIN | AUTO));
  EXPECT(keeps(MAIN | INV, MAIN | AUTO));
}

static void a_repeated_mode_is_modifier_only() {
  const uint8_t modes[] = {MAIN, MAIN | AUTO, MAIN | AUTO | INV, PROG,
                           EXT, BOOST, BOOST | INV, NONE};
  for (uint8_t m : modes) EXPECT(keeps(m, m));
}

static void a_base_mode_change_takes_the_full_setup() {
  EXPECT(!keeps(MAIN, PROG));
  EXPECT(!keeps(PROG, MAIN));
  EXPECT(!keeps(MAIN | AUTO, PROG));
  EXPECT(!keeps(MAIN, DC));
  EXPECT(!keeps(MAIN | INV, DC | INV));
  EXPECT(!keeps(NONE, MAIN));
  EXPECT(!keeps(MAIN, EXT));
  EXPECT(!keeps(MAIN, BOOST));
}

static void a_dc_track_with_the_same_address_is_modifier_only() {
  EXPECT(isModifierOnlyChange(DC, DC, 3, 3));
  EXPECT(isModifierOnlyChange(DC, DC | INV, 3, 3));
  EXPECT(isModifierOnlyChange(DC | INV, DC, 3, 3));
}

static void a_dc_track_with_a_new_address_takes_the_full_setup() {
  EXPECT(!isModifierOnlyChange(DC, DC, 3, 4));
  EXPECT(!isModifierOnlyChange(DC, DC | INV, 3, 4));
  EXPECT(!isModifierOnlyChange(DC, DC, 0, 3));
}

static void the_dc_address_does_not_matter_on_a_dcc_track() {
  EXPECT(isModifierOnlyChange(MAIN, MAIN | INV, 0, 5));
}

#define RUN(test)                                                         \
  do {                                                                    \
    current = #test;                                                      \
    test();                                                               \
  } while (0)

int main() {
  RUN(inv_on_is_modifier_only);
  RUN(inv_off_is_modifier_only);
  RUN(auto_on_is_modifier_only);
  RUN(auto_off_is_modifier_only);
  RUN(main_auto_and_main_auto_inv_swap_without_setup);
  RUN(a_repeated_mode_is_modifier_only);
  RUN(a_base_mode_change_takes_the_full_setup);
  RUN(a_dc_track_with_the_same_address_is_modifier_only);
  RUN(a_dc_track_with_a_new_address_takes_the_full_setup);
  RUN(the_dc_address_does_not_matter_on_a_dcc_track);
  if (failures) {
    std::printf("%d failed\n", failures);
    return 1;
  }
  std::printf("track mode: all passed\n");
  return 0;
}
