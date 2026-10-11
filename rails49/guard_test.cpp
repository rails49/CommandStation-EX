// Native test of the Guard's per-tick decision, guardDecide(). Run with
// rails49/test-guard.sh. Each test feeds a sequence of ticks and asserts on
// which track is reversed or cut on which tick. Words as in CONTEXT.md.

#include "../GuardDecision.h"
#include <cstdio>
#include <cstring>

static const GuardTimes times = {100, 1000, 4000, 20000};
static const int TRIP = 1000; // raw counts

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

enum { A, B, C, D };

// Four tracks as on rails49 by default: A a plain MAIN track, B PROG, C and
// D AUTO. Carries the Guard's state from tick to tick as the ISR does, and
// lets a test set what the hardware reports.
struct Station {
  GuardIn in[4];
  GuardOut out[4];
  unsigned long now = 1000000;
  bool reversed[4] = {};
  bool cut[4] = {};

  Station() {
    for (int t = 0; t < 4; t++) {
      in[t].active = true;
      in[t].armAt = now; // powered on long ago, the Inrush Window is over
      in[t].holdUntil = now;
    }
    in[B].isProg = true;
    in[C].isAuto = in[D].isAuto = true;
  }

  // Power comes on now: the Inrush Window starts.
  void powerOn(int t, unsigned long inrushUs = 100000) {
    in[t].active = true;
    in[t].armAt = now + inrushUs;
  }

  // The ADC reads track t on the next tick. Until it reads it again, the
  // Guard sees the same sample.
  void sample(int t, int raw) {
    in[t].sampled = true;
    in[t].lastRaw = raw;
    in[t].currentOver = raw >= TRIP;
  }

  // As the round robin over four tracks does: a sample of t, then four
  // ticks. Returns true if t was reversed on one of them.
  bool sampleAndWait(int t, int raw) {
    sample(t, raw);
    bool any = false;
    for (int i = 0; i < 4; i++) {
      tick();
      any |= reversed[t];
    }
    return any;
  }

  // One tick, then what the ISR keeps for the next.
  void tick() {
    guardDecide(in, out, 4, now, times);
    for (int t = 0; t < 4; t++) {
      reversed[t] = out[t].reverse;
      cut[t] = out[t].cut;
      in[t].acc = out[t].acc;
      in[t].overCount = out[t].overCount;
      in[t].sampled = false;
      if (out[t].hold) in[t].holdUntil = now + times.settleUs;
      if (out[t].reverse) {
        in[t].hasReversed = true;
        in[t].lastReversal = now;
      }
      if (out[t].cut) in[t].active = false;
    }
    now += times.tickUs;
  }

  // Ticks until track t is reversed or cut or n ticks have passed.
  // Returns the number of ticks taken, n + 1 if nothing happened.
  int ticksUntil(bool (Station::*what)[4], int t, int n) {
    for (int i = 1; i <= n; i++) {
      tick();
      if ((this->*what)[t]) return i;
    }
    return n + 1;
  }
  bool anyAction() {
    for (int t = 0; t < 4; t++)
      if (reversed[t] || cut[t]) return true;
    return false;
  }
};

#define TEST(name) static void name()
#define RUN(name) (current = #name, name())

TEST(a_fault_on_an_auto_track_reverses_it_at_once) {
  Station s;
  s.in[D].fault = true;
  s.tick();
  EXPECT(s.reversed[D]);
  EXPECT(!s.reversed[C]);
  EXPECT(!s.cut[D]);
}

TEST(nothing_happens_during_the_hold_after_a_reversal) {
  Station s;
  s.in[D].fault = true;
  s.in[A].fault = true;
  s.tick();
  EXPECT(s.reversed[D]);
  for (unsigned long us = times.tickUs; us < times.settleUs; us += times.tickUs) {
    s.tick();
    EXPECT(!s.anyAction());
  }
}

TEST(a_fault_on_a_plain_track_is_an_overload_after_the_fast_trip_time) {
  Station s;
  s.in[C].active = s.in[D].active = false; // no loop to reverse
  s.in[A].fault = true;
  EXPECT(s.ticksUntil(&Station::cut, A, 20) == 10);
}

TEST(a_reversal_that_does_not_help_ends_in_an_overload) {
  Station s;
  s.in[C].active = false;
  s.in[D].fault = true;
  s.tick();
  EXPECT(s.reversed[D]);
  // past the hold D is locked out, the fault stays: cut after 1 ms
  EXPECT(s.ticksUntil(&Station::cut, D, 100) == 40 + 10 - 1);
  EXPECT(!s.reversed[D]);
}

TEST(current_is_ignored_in_the_inrush_window) {
  Station s;
  s.powerOn(A);
  s.sample(A, 5000);
  for (int i = 0; i < 999; i++) {
    s.tick();
    EXPECT(!s.anyAction());
  }
}

TEST(current_on_a_prog_track_is_left_to_the_main_loop) {
  Station s;
  s.sample(B, 5000);
  for (int i = 0; i < 100; i++) {
    s.tick();
    EXPECT(!s.anyAction());
  }
}

TEST(a_fault_on_a_prog_track_is_an_overload_and_reverses_no_loop) {
  Station s;
  s.in[B].fault = true;
  EXPECT(s.ticksUntil(&Station::cut, B, 20) == 10);
  EXPECT(!s.in[C].hasReversed && !s.in[D].hasReversed);
}

TEST(an_inactive_track_is_never_acted_on) {
  Station s;
  s.in[D].active = false;
  s.in[D].fault = true;
  for (int i = 0; i < 100; i++) {
    s.tick();
    EXPECT(!s.anyAction());
  }
}

TEST(one_sample_over_does_not_reverse_an_auto_track) {
  Station s;
  EXPECT(!s.sampleAndWait(D, 5000));
  EXPECT(!s.sampleAndWait(D, 500));
  EXPECT(!s.in[D].hasReversed);
}

TEST(two_consecutive_samples_over_reverse_an_auto_track) {
  Station s;
  EXPECT(!s.sampleAndWait(D, 5000));
  s.sample(D, 5000);
  s.tick();
  EXPECT(s.reversed[D]);
  EXPECT(!s.reversed[C]);
}

TEST(a_sample_under_the_limit_resets_the_count) {
  Station s;
  EXPECT(!s.sampleAndWait(D, 5000));
  EXPECT(!s.sampleAndWait(D, 500));
  EXPECT(!s.sampleAndWait(D, 5000));
  EXPECT(!s.sampleAndWait(D, 500));
  EXPECT(!s.in[D].hasReversed);
}

TEST(one_sample_over_on_a_plain_track_reverses_no_loop) {
  Station s;
  EXPECT(!s.sampleAndWait(A, 5000));
  EXPECT(!s.sampleAndWait(A, 500));
  EXPECT(!s.in[C].hasReversed && !s.in[D].hasReversed);
}

TEST(two_samples_over_on_a_plain_track_reverse_a_loop) {
  Station s;
  s.sampleAndWait(A, 5000);
  s.sample(A, 5000);
  s.tick();
  EXPECT(s.reversed[C] || s.reversed[D]);
}

TEST(a_sustained_soft_short_on_an_auto_track_reverses_before_the_cut) {
  Station s;
  s.in[C].active = false;
  EXPECT(!s.sampleAndWait(D, 5000));
  s.sample(D, 5000);
  s.tick();
  EXPECT(s.reversed[D]);
  EXPECT(!s.cut[D]);
}

int main() {
  RUN(a_fault_on_an_auto_track_reverses_it_at_once);
  RUN(nothing_happens_during_the_hold_after_a_reversal);
  RUN(a_fault_on_a_plain_track_is_an_overload_after_the_fast_trip_time);
  RUN(a_reversal_that_does_not_help_ends_in_an_overload);
  RUN(current_is_ignored_in_the_inrush_window);
  RUN(current_on_a_prog_track_is_left_to_the_main_loop);
  RUN(a_fault_on_a_prog_track_is_an_overload_and_reverses_no_loop);
  RUN(an_inactive_track_is_never_acted_on);
  RUN(one_sample_over_does_not_reverse_an_auto_track);
  RUN(two_consecutive_samples_over_reverse_an_auto_track);
  RUN(a_sample_under_the_limit_resets_the_count);
  RUN(one_sample_over_on_a_plain_track_reverses_no_loop);
  RUN(two_samples_over_on_a_plain_track_reverse_a_loop);
  RUN(a_sustained_soft_short_on_an_auto_track_reverses_before_the_cut);
  if (failures) {
    std::printf("%d failed\n", failures);
    return 1;
  }
  std::printf("guard: all passed\n");
  return 0;
}
