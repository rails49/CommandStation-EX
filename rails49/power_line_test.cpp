// Native test of what the power lines report for a track, PowerLine.h: a
// track in ALERT reads <p0 x>, and a <p1 x> is then due (#78). Run with
// rails49/test-guard.sh. Words as in CONTEXT.md.

#include "../PowerLine.h"
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

static void a_track_that_is_on_reads_1() {
  bool due = false;
  EXPECT(readPowerLine(true, false, due) == '1');
  EXPECT(!due);
}

static void a_track_in_alert_reads_0_and_sets_p1_due() {
  bool due = false;
  EXPECT(readPowerLine(false, true, due) == '0');
  EXPECT(due);
}

static void a_track_off_or_in_overload_reads_0() {
  bool due = false;
  EXPECT(readPowerLine(false, false, due) == '0');
  EXPECT(!due);
  // after an Overload's <p0 x> the <p1 x> stays due
  due = true;
  EXPECT(readPowerLine(false, false, due) == '0');
  EXPECT(due);
}

#define RUN(test)                                                         \
  do {                                                                    \
    current = #test;                                                      \
    test();                                                               \
  } while (0)

int main() {
  RUN(a_track_that_is_on_reads_1);
  RUN(a_track_in_alert_reads_0_and_sets_p1_due);
  RUN(a_track_off_or_in_overload_reads_0);
  if (failures) {
    std::printf("%d failed\n", failures);
    return 1;
  }
  std::printf("power line: all passed\n");
  return 0;
}
