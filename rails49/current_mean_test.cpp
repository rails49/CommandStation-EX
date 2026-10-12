// Native test of the mean <JI> reports, CurrentMean.h: the mean over the last
// complete window, which a read does not reset, so every client polling <JI>
// reads the same value at the same moment (#52). Run with
// rails49/test-guard.sh. Words as in CONTEXT.md.

#include "../CurrentMean.h"
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

static void two_reads_in_a_row_return_the_same_mean() {
  CurrentMean m;
  m.add(100, 0);
  m.add(300, 1000);
  m.add(500, W);  // closes the window: mean 200
  int first = -1, second = -1;
  EXPECT(m.mean(first));
  EXPECT(m.mean(second));
  EXPECT(first == 200);
  EXPECT(second == 200);
}

static void samples_in_the_open_window_do_not_change_the_mean() {
  CurrentMean m;
  m.add(100, 0);
  m.add(500, W);
  m.add(900, W + 1000);
  int mean = -1;
  EXPECT(m.mean(mean));
  EXPECT(mean == 100);
}

static void the_next_window_replaces_the_mean() {
  CurrentMean m;
  m.add(100, 0);
  m.add(500, W);
  m.add(700, W + 1000);
  m.add(0, 2 * W);
  int mean = -1;
  EXPECT(m.mean(mean));
  EXPECT(mean == 600);
}

static void before_the_first_window_closes_the_mean_is_of_the_samples_so_far() {
  CurrentMean m;
  m.add(100, 0);
  m.add(300, 1000);
  int first = -1, second = -1;
  EXPECT(m.mean(first));
  EXPECT(m.mean(second));
  EXPECT(first == 200);
  EXPECT(second == 200);
}

static void without_samples_there_is_no_mean() {
  CurrentMean m;
  int mean = -1;
  EXPECT(!m.mean(mean));
  m.add(100, 0);
  m.add(100, W);
  m.reset();
  EXPECT(!m.mean(mean));
}

static void the_window_survives_micros_wrapping() {
  CurrentMean m;
  unsigned long start = (unsigned long)-1000;
  m.add(100, start);
  m.add(300, start + W / 2);
  m.add(500, start + W);
  int mean = -1;
  EXPECT(m.mean(mean));
  EXPECT(mean == 200);
}

#define RUN(test)                                                         \
  do {                                                                    \
    current = #test;                                                      \
    test();                                                               \
  } while (0)

int main() {
  RUN(two_reads_in_a_row_return_the_same_mean);
  RUN(samples_in_the_open_window_do_not_change_the_mean);
  RUN(the_next_window_replaces_the_mean);
  RUN(before_the_first_window_closes_the_mean_is_of_the_samples_so_far);
  RUN(without_samples_there_is_no_mean);
  RUN(the_window_survives_micros_wrapping);
  if (failures) {
    std::printf("%d failed\n", failures);
    return 1;
  }
  std::printf("current mean: all passed\n");
  return 0;
}
