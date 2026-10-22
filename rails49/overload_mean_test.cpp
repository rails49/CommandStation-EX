// Native test of what entering Overload does to the mean <JI> reports,
// OverloadMean.h: it clears the mean, so after a retry <JI> reports the
// current since the retry, not the current that caused the Overload
// (#59). Run with rails49/test-guard.sh. Words as in GLOSSARY.md.

#include "../OverloadMean.h"
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

static const unsigned long W = CurrentMean::WINDOW_US;
// POWER_SAMPLE_OVERLOAD_WAIT, the first retry
static const unsigned long RETRY_WAIT = 40000UL;

// A track as <JI> sees it: the mean, and whether it is in Overload, where
// TrackManager::reportCurrent() reports -1.
struct Track {
  CurrentMean mean;
  bool overload = false;

  void sample(unsigned int raw, unsigned long now) {
    if (!overload)
      mean.add(raw, now);
  }
  void enterOverload() {
    cutForOverload(mean, [this]() { overload = true; });
  }
  void retry() { overload = false; }
  int reported() const {
    int m = 0;
    if (overload)
      return -1;
    return mean.mean(m) ? m : 0;
  }
};

static void after_a_retry_the_mean_is_of_the_samples_since() {
  Track t;
  t.sample(100, 0);
  t.sample(3000, 1000);
  t.enterOverload();
  EXPECT(t.reported() == -1);
  t.retry();
  t.sample(100, 1000 + RETRY_WAIT);
  t.sample(300, 2000 + RETRY_WAIT);
  EXPECT(t.reported() == 200);
}

static void a_retry_after_the_window_does_not_report_the_overload() {
  Track t;
  t.sample(3000, 0);
  t.sample(3000, 1000);
  t.enterOverload();
  t.retry();
  t.sample(100, W + 1000);  // would close the window opened before the Overload
  EXPECT(t.reported() == 100);
}

static void a_closed_window_before_the_overload_is_not_reported() {
  Track t;
  t.sample(3000, 0);
  t.sample(500, W);  // closes a window: mean 3000
  t.enterOverload();
  t.retry();
  t.sample(100, W + RETRY_WAIT);
  EXPECT(t.reported() == 100);
}

#define RUN(test)                                                         \
  do {                                                                    \
    current = #test;                                                      \
    test();                                                               \
  } while (0)

int main() {
  RUN(after_a_retry_the_mean_is_of_the_samples_since);
  RUN(a_retry_after_the_window_does_not_report_the_overload);
  RUN(a_closed_window_before_the_overload_is_not_reported);
  if (failures) {
    std::printf("%d failed\n", failures);
    return 1;
  }
  std::printf("overload mean: all passed\n");
  return 0;
}
