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

  // How power comes on: the host powers the track ON from OFF or from
  // OVERLOAD, or the main loop retries, moving it from OVERLOAD to ALERT.
  enum How { HOST_FROM_OFF, HOST_FROM_OVERLOAD, RETRY };

  // Power comes on now, as the driver arms the Guard. byCurrent: the last
  // Overload was a current Overload, not a Fault.
  void powerOn(int t, How how = HOST_FROM_OFF, bool byCurrent = false,
               unsigned long inrushUs = 100000) {
    in[t].active = true;
    in[t].armAt =
        guardInrushWindow(how == RETRY, byCurrent) ? now + inrushUs : now;
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

TEST(a_reversed_track_is_not_reversed_again_within_the_lockout) {
  Station s;
  s.in[C].active = false;
  s.in[D].fault = true;
  s.tick();
  EXPECT(s.reversed[D]);
  s.in[D].fault = false;
  // past the hold, inside the lockout: a short shorter than the fast
  // trip time neither reverses nor cuts
  while (s.now - s.in[D].lastReversal < times.reinvertUs / 2) s.tick();
  s.in[D].fault = true;
  for (int i = 0; i < 5; i++) {
    s.tick();
    EXPECT(!s.anyAction());
  }
  s.in[D].fault = false;
  // past the lockout the next short reverses again
  while (s.now - s.in[D].lastReversal < times.reinvertUs) s.tick();
  s.in[D].fault = true;
  s.tick();
  EXPECT(s.reversed[D]);
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

TEST(a_host_power_on_keeps_the_inrush_window) {
  Station s;
  s.powerOn(A, Station::HOST_FROM_OFF);
  s.sample(A, 5000);
  EXPECT(s.ticksUntil(&Station::cut, A, 999) == 1000);
}

TEST(a_host_power_on_from_overload_after_a_current_overload_keeps_the_inrush_window) {
  Station s;
  s.powerOn(A, Station::HOST_FROM_OVERLOAD, true);
  s.sample(A, 5000);
  EXPECT(s.ticksUntil(&Station::cut, A, 999) == 1000);
}

TEST(a_retry_after_a_fault_keeps_the_inrush_window) {
  Station s;
  s.powerOn(A, Station::RETRY, false);
  s.sample(A, 5000);
  EXPECT(s.ticksUntil(&Station::cut, A, 999) == 1000);
}

TEST(a_retry_after_a_current_overload_is_cut_within_the_fast_trip_time) {
  Station s;
  s.powerOn(A, Station::RETRY, true);
  s.sample(A, 5000);
  EXPECT(s.ticksUntil(&Station::cut, A, 20) == 10);
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

TEST(over_under_over_does_not_reverse_but_the_next_over_does) {
  Station s;
  EXPECT(!s.sampleAndWait(D, 5000));
  EXPECT(!s.sampleAndWait(D, 500));
  EXPECT(!s.sampleAndWait(D, 5000));
  s.sample(D, 5000);
  s.tick();
  EXPECT(s.reversed[D]);
}

TEST(a_fault_reverses_on_its_first_tick_whatever_the_count) {
  Station s;
  EXPECT(!s.sampleAndWait(D, 5000));
  s.in[D].fault = true;
  s.tick();
  EXPECT(s.reversed[D]);
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

TEST(a_fault_on_a_plain_track_reverses_the_loop_with_the_higher_sample) {
  Station s;
  s.sampleAndWait(C, 300);
  s.sampleAndWait(D, 700);
  s.in[A].fault = true;
  s.tick();
  EXPECT(s.reversed[D]);
  EXPECT(!s.reversed[C]);
}

TEST(on_a_tie_the_lower_track_letter_reverses) {
  Station s;
  s.sampleAndWait(C, 500);
  s.sampleAndWait(D, 500);
  s.in[A].fault = true;
  s.tick();
  EXPECT(s.reversed[C]);
  EXPECT(!s.reversed[D]);
}

TEST(the_other_loop_reverses_if_the_fault_persists_past_the_hold) {
  Station s;
  s.sampleAndWait(C, 300);
  s.sampleAndWait(D, 700);
  s.in[A].fault = true;
  s.tick();
  EXPECT(s.reversed[D]);
  // the hold, then C, and no Overload on the way
  EXPECT(s.ticksUntil(&Station::reversed, C, 100) == 40);
  EXPECT(!s.in[A].hasReversed && s.in[A].active);
  EXPECT(!s.reversed[D]);
}

TEST(a_wrong_first_guess_still_ends_in_a_reversal_not_a_cut) {
  Station s;
  s.sampleAndWait(D, 700);
  s.in[A].fault = true;
  s.tick();
  EXPECT(s.reversed[D]);
  s.ticksUntil(&Station::reversed, C, 100);
  s.in[A].fault = false; // C was the loop with the loco on its gap
  for (int i = 0; i < 400; i++) {
    s.tick();
    EXPECT(!s.anyAction());
  }
}

TEST(an_auto_track_that_is_itself_over_reverses_whatever_the_plain_track_sees) {
  Station s;
  s.sampleAndWait(C, 900);
  s.in[A].fault = true;
  s.in[D].fault = true;
  s.tick();
  EXPECT(s.reversed[D]);
  EXPECT(!s.reversed[C]);
}

TEST(a_loop_that_cannot_reverse_is_passed_over_whatever_its_sample) {
  Station s;
  s.sampleAndWait(C, 700);
  s.sampleAndWait(D, 300);
  s.in[C].active = false;
  s.in[A].fault = true;
  s.tick();
  EXPECT(s.reversed[D]);
  EXPECT(!s.reversed[C]);
}

TEST(an_auto_track_over_by_current_reverses_whatever_the_plain_track_sees) {
  for (int aFault = 0; aFault < 2; aFault++) {
    Station s;
    s.sampleAndWait(C, 900);
    s.in[A].fault = aFault;
    // with a Fault on A, D's first sample over makes it the pick
    if (!aFault) s.sampleAndWait(D, 5000);
    s.sample(D, 5000);
    s.tick();
    EXPECT(s.reversed[D]);
    EXPECT(!s.in[C].hasReversed);
  }
}

TEST(one_sample_over_on_a_loop_does_not_block_the_pick_for_a_plain_track_short) {
  Station s;
  s.sampleAndWait(C, 300);
  s.in[A].fault = true;
  s.sample(D, 1500);
  s.tick();
  EXPECT(s.reversed[D]);
  EXPECT(!s.reversed[C]);
}

TEST(one_sample_over_on_a_loop_lets_the_loop_with_the_higher_sample_reverse) {
  Station s;
  s.sampleAndWait(C, 2000);
  s.in[A].fault = true;
  s.sample(D, 1500);
  s.tick();
  EXPECT(s.reversed[C]);
  EXPECT(!s.reversed[D]);
}

int main() {
  RUN(a_fault_on_an_auto_track_reverses_it_at_once);
  RUN(nothing_happens_during_the_hold_after_a_reversal);
  RUN(a_fault_on_a_plain_track_is_an_overload_after_the_fast_trip_time);
  RUN(a_reversal_that_does_not_help_ends_in_an_overload);
  RUN(a_reversed_track_is_not_reversed_again_within_the_lockout);
  RUN(current_is_ignored_in_the_inrush_window);
  RUN(a_host_power_on_keeps_the_inrush_window);
  RUN(a_host_power_on_from_overload_after_a_current_overload_keeps_the_inrush_window);
  RUN(a_retry_after_a_fault_keeps_the_inrush_window);
  RUN(a_retry_after_a_current_overload_is_cut_within_the_fast_trip_time);
  RUN(current_on_a_prog_track_is_left_to_the_main_loop);
  RUN(a_fault_on_a_prog_track_is_an_overload_and_reverses_no_loop);
  RUN(an_inactive_track_is_never_acted_on);
  RUN(one_sample_over_does_not_reverse_an_auto_track);
  RUN(two_consecutive_samples_over_reverse_an_auto_track);
  RUN(a_sample_under_the_limit_resets_the_count);
  RUN(over_under_over_does_not_reverse_but_the_next_over_does);
  RUN(a_fault_reverses_on_its_first_tick_whatever_the_count);
  RUN(one_sample_over_on_a_plain_track_reverses_no_loop);
  RUN(two_samples_over_on_a_plain_track_reverse_a_loop);
  RUN(a_sustained_soft_short_on_an_auto_track_reverses_before_the_cut);
  RUN(a_fault_on_a_plain_track_reverses_the_loop_with_the_higher_sample);
  RUN(on_a_tie_the_lower_track_letter_reverses);
  RUN(the_other_loop_reverses_if_the_fault_persists_past_the_hold);
  RUN(a_wrong_first_guess_still_ends_in_a_reversal_not_a_cut);
  RUN(an_auto_track_that_is_itself_over_reverses_whatever_the_plain_track_sees);
  RUN(a_loop_that_cannot_reverse_is_passed_over_whatever_its_sample);
  RUN(an_auto_track_over_by_current_reverses_whatever_the_plain_track_sees);
  RUN(one_sample_over_on_a_loop_does_not_block_the_pick_for_a_plain_track_short);
  RUN(one_sample_over_on_a_loop_lets_the_loop_with_the_higher_sample_reverse);
  if (failures) {
    std::printf("%d failed\n", failures);
    return 1;
  }
  std::printf("guard: all passed\n");
  return 0;
}
