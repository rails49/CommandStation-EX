// Native test of when MotorDriver::setPower() drives the power pin and arms
// the Guard, PowerSwitch.h: only when power goes on or off, so a Guard cut
// stays cut through ON to ALERT and back (#83). Run with
// rails49/test-guard.sh. Words as in CONTEXT.md.

#include "../PowerSwitch.h"
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

enum Mode { OFF, ON, OVERLOAD, ALERT };

// A track the Guard looks after: its power mode, its power pin and guardActive.
struct Track {
  Mode mode = OFF;
  bool pin = false;
  bool guardActive = false;

  void setPower(Mode m) {
    bool on = m == ON || m == ALERT;
    bool wasOn = mode == ON || mode == ALERT;
    if (powerSwitches(wasOn, on)) {
      pin = on;
      guardActive = on;
    }
    mode = m;
  }

  void guardCut() {
    pin = false;
    guardActive = false;
  }
};

static void a_guard_cut_stays_cut_when_the_track_goes_to_alert() {
  Track t;
  t.setPower(ON);
  t.guardCut();
  t.setPower(ALERT);
  EXPECT(!t.pin);
  EXPECT(!t.guardActive);
}

static void a_guard_cut_stays_cut_when_the_track_goes_back_on() {
  Track t;
  t.setPower(ON);
  t.setPower(ALERT);
  t.guardCut();
  t.setPower(ON);
  EXPECT(!t.pin);
  EXPECT(!t.guardActive);
}

static void a_host_power_on_drives_the_pin_and_arms_the_guard() {
  Track t;
  t.setPower(ON);
  EXPECT(t.pin);
  EXPECT(t.guardActive);
}

static void a_retry_drives_the_pin_and_arms_the_guard() {
  Track t;
  t.setPower(ON);
  t.setPower(OVERLOAD);
  EXPECT(!t.pin);
  EXPECT(!t.guardActive);
  t.setPower(ALERT);
  EXPECT(t.pin);
  EXPECT(t.guardActive);
}

#define RUN(test)                                                         \
  do {                                                                    \
    current = #test;                                                      \
    test();                                                               \
  } while (0)

int main() {
  RUN(a_guard_cut_stays_cut_when_the_track_goes_to_alert);
  RUN(a_guard_cut_stays_cut_when_the_track_goes_back_on);
  RUN(a_host_power_on_drives_the_pin_and_arms_the_guard);
  RUN(a_retry_drives_the_pin_and_arms_the_guard);
  if (failures) {
    std::printf("%d failed\n", failures);
    return 1;
  }
  std::printf("power switch: all passed\n");
  return 0;
}
