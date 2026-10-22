// Native test of the order MotorDriver::checkPowerOverload() follows when the
// main loop sees a short on a track that is ON: a Reversal is broadcast while
// the track is still ON, so the power lines say <p1 x> (#51). Run with
// rails49/test-guard.sh. Words as in GLOSSARY.md.

#include "../MainLoopReversal.h"
#include "../PowerSwitch.h"
#include <cstdio>
#include <string>

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

// A track as the main loop sees it: ON or ALERT, a Polarity, and every
// Broadcast of its track state as the client reads it.
struct Track {
  bool alert = false;
  bool inverted = false;
  std::string broadcasts;

  void enterAlert(bool reversal) {
    alertFromOn(
        reversal,
        [this]() {
          inverted = !inverted;
          // <p1 x> only for ON, as TrackManager::getPower() writes it
          broadcasts += inverted ? "MAIN A I," : "MAIN A,";
          broadcasts += alert ? "p0;" : "p1;";
        },
        [this]() { alert = true; });
  }
};

// An AUTO track the Guard looks after, with the two members of MotorDriver
// that mainLoopReverses() may read: guardEligible() follows the Track Mode,
// guardActive is cleared by a Guard cut.
struct GuardedTrack {
  bool eligible = true;
  bool guardActive = true;
  bool pin = true;
  bool inverted = false;
  int mainLoopReversals = 0;

  bool guardEligible() { return eligible; }

  void guardReversesAndCuts() {
    inverted = !inverted;
    pin = false;
    guardActive = false;
  }

  // ON to ALERT, as MotorDriver::setPower() does it
  void enterAlert() {
    if (powerSwitches(true, true)) {
      pin = true;
      guardActive = guardEligible();
    }
  }

  void mainLoopSeesShort() {
    alertFromOn(mainLoopReverses(true, *this),
                [this]() {
                  inverted = !inverted;
                  mainLoopReversals++;
                },
                [this]() { enterAlert(); });
  }
};

static void a_reversal_broadcasts_p1() {
  Track t;
  t.enterAlert(true);
  EXPECT(t.broadcasts == "MAIN A I,p1;");
  EXPECT(t.alert);
}

static void the_next_reversal_broadcasts_p1_and_normal_polarity() {
  Track t;
  t.inverted = true;
  t.enterAlert(true);
  EXPECT(t.broadcasts == "MAIN A,p1;");
}

static void an_alert_without_a_reversal_broadcasts_nothing() {
  Track t;
  t.enterAlert(false);
  EXPECT(t.broadcasts.empty());
  EXPECT(!t.inverted);
  EXPECT(t.alert);
}

static void a_guard_cut_does_not_let_the_main_loop_reverse_again() {
  GuardedTrack t;
  t.guardReversesAndCuts();
  t.mainLoopSeesShort();
  EXPECT(t.mainLoopReversals == 0);
  EXPECT(t.inverted);
  EXPECT(!t.pin);
  EXPECT(!t.guardActive);
}

static void the_main_loop_reverses_a_track_the_guard_does_not_look_after() {
  GuardedTrack t;
  t.eligible = false;
  t.guardActive = false;
  t.mainLoopSeesShort();
  EXPECT(t.mainLoopReversals == 1);
  EXPECT(t.inverted);
}

#define RUN(test)                                                         \
  do {                                                                    \
    current = #test;                                                      \
    test();                                                               \
  } while (0)

int main() {
  RUN(a_reversal_broadcasts_p1);
  RUN(the_next_reversal_broadcasts_p1_and_normal_polarity);
  RUN(an_alert_without_a_reversal_broadcasts_nothing);
  RUN(a_guard_cut_does_not_let_the_main_loop_reverse_again);
  RUN(the_main_loop_reverses_a_track_the_guard_does_not_look_after);
  if (failures) {
    std::printf("%d failed\n", failures);
    return 1;
  }
  std::printf("main loop reversal: all passed\n");
  return 0;
}
