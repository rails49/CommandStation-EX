// Native test of the rule TrackManager::reportedMode() uses: which tracks
// report the Polarity their hardware has rather than the INV the host set.
// Run with rails49/test-guard.sh. Words as in GLOSSARY.md.

#include "../TrackModeReport.h"
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

// The mode words as MotorDriver.h has them on ESP32.
enum : uint8_t {
  NONE = 1, MAIN = 2, PROG = 4, DC = 8, EXT = 16, BOOST = 32,
  INV = 64, AUTO = 128,
};

static void a_reversed_main_auto_track_reports_inv() {
  EXPECT(reportedModeWord(MAIN | AUTO, true) == (MAIN | AUTO | INV));
  EXPECT(reportedModeWord(MAIN | AUTO | INV, false) == (MAIN | AUTO));
}

static void a_reversed_boost_auto_track_reports_inv() {
  EXPECT(reportedModeWord(BOOST | AUTO, true) == (BOOST | AUTO | INV));
}

static void the_next_reversal_of_boost_auto_clears_inv() {
  EXPECT(reportedModeWord(BOOST | AUTO, false) == (BOOST | AUTO));
  EXPECT(reportedModeWord(BOOST | AUTO | INV, false) == (BOOST | AUTO));
}

static void a_boost_track_inverted_by_the_host_reports_inv() {
  EXPECT(reportedModeWord(BOOST | INV, true) == (BOOST | INV));
  EXPECT(reportedModeWord(BOOST, false) == BOOST);
}

static void other_modes_report_the_mode_as_set() {
  const uint8_t modes[] = {NONE, PROG, DC, DC | INV, EXT};
  for (uint8_t m : modes) {
    EXPECT(reportedModeWord(m, false) == m);
    EXPECT(reportedModeWord(m, true) == m);
  }
}

#define RUN(test)                                                         \
  do {                                                                    \
    current = #test;                                                      \
    test();                                                               \
  } while (0)

int main() {
  RUN(a_reversed_main_auto_track_reports_inv);
  RUN(a_reversed_boost_auto_track_reports_inv);
  RUN(the_next_reversal_of_boost_auto_clears_inv);
  RUN(a_boost_track_inverted_by_the_host_reports_inv);
  RUN(other_modes_report_the_mode_as_set);
  if (failures) {
    std::printf("%d failed\n", failures);
    return 1;
  }
  std::printf("track report: all passed\n");
  return 0;
}
