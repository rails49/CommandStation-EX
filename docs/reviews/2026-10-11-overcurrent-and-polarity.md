# Overcurrent guard, power cut and auto-reverse: code review

Repo: rails49/CommandStation-EX, branch `runtime-current-limits` at 4f5c4b7
(v5.6.4-rails49.16). Reviewed 2026-10-11.

Scope: the fast overcurrent guard (PRs #2 and #8), the overload state machine,
track mode and polarity handling on ESP32, and the layout script in
`dccex/src/dccex/sample.py` that drives polarity. The three requirements:

1. cut power on a short as fast as possible;
2. auto-reverse a MAIN_AUTO track on the current spike a wrong polarity causes;
3. keep working when the host sets a track's polarity on purpose.

## Method

Code reading only. No firmware was changed and nothing was flashed. There is
no host-side test seam: the guard reads ESP32 registers directly, and the only
verification so far has been a train on the layout. Finding 1 rests on the
contract of `esp_rom_gpio_connect_out_signal`, whose `out_inv` argument sets
the invert bit, and has not been confirmed on hardware. Recommendation 1 below
turns it into a scripted check.

## Findings, most serious first

### 1. Any `<=` command un-inverts every other MAIN track

Each mode command on ESP32 re-runs the waveform setup
(`TrackManager.cpp:419`), which re-attaches every MAIN track's signal pin to
the RMT (`DCCWaveformRMT.cpp:47`) with the invert bit cleared
(`DCCRMT.cpp:274`, `inverted=false`). Only the commanded track gets its
inversion re-applied by the second `setMode()`. For every other track the
software flag `invertPhase` now disagrees with the hardware.

Consequences on rails49:

- C is MAIN_AUTO. After C has auto-inverted an odd number of times, the next
  turnout 12 change sends `<= D MAIN_AUTO>`, which resets C's hardware to
  non-inverted while its flag stays inverted. At C's next reversal the toggle
  (`MotorDriver.h:332`) flips the flag to false and writes non-inverted, a
  no-op. The short persists, the 4 ms hold expires, the guard cuts C and the
  neighbouring track after 1 ms. Power returns 40 ms later and the second
  invert works. Symptom: a random stop at the C gap after turnout 12 moved.
- The host's `<= D INV>` is lost if any later `<=` names another track.
- A plain MAIN_INV track is affected the same way. This is an upstream bug;
  AUTO makes it visible.
- The guard ISR and the main loop both read-modify-write the same GPIO
  register, so a mode command racing an invert can also desync the flag.
  Rare, same fix.

Fix: after the waveform setup in `setTrackMode`, re-apply every track's
current `invertPhase` to hardware with interrupts off, or make the RMT pin
attach preserve the invert bit. Keep the commanded track's reset to its
configured INV, since that is what the host asked for.

### 2. The host cannot learn the polarity it is supposed to know

`<=>` reports `MAIN A` whether or not INV is set (`TrackManager.cpp:524`). An
auto-invert is announced only as a `<* TRACK D INVERT (fast) *>` diagnostic
line. After the first auto-reversal the host's belief about D is stale, and
the next turnout change sets polarity from that stale belief. Requirement 3
cannot be met without feedback.

Fix: broadcast the track state on every invert and include INV in the name of
an AUTO track (for example `MAIN A I`), or add a polarity field the script can
read.

### 3. Setting polarity on purpose costs one or two power cuts on D

The script sends `<= D MAIN_AUTO>`, then `<= D INV>`, then `<1 D>`. Each real
mode change powers D off, so a loco in Airolo stops and restarts, and each
restart opens a fresh 100 ms inrush window in which only the fault pin is
watched. A polarity flip on a DCC track has none of the runaway hazard the
power-off exists for.

Fix: let `<= D INV>` on an AUTO track change polarity without a power cut, and
have the script send one command instead of two.

### 4. A fault pin at power-on is cut after 1 ms, with no grace and no throttled charging

The guard watches the fault pin from power on; only the current check is
armed after the inrush window (`MotorDriver.cpp:251`). ADR-0004 removed the
brake PWM on guarded tracks. A loco whose stay-alive makes the DRV8874 trip at
power-on never powers up: every retry repeats the 1 ms cut with a growing
back-off. Upstream gave such a loco 100 ms and throttled charging.

Confidence: medium. The DRV8874's current regulation may absorb the inrush,
and the layout's locos power up today.

Fix: during the inrush window after a host power-on, require fault pin and
current over limit together before cutting, or keep the brake PWM for that
window only.

### 5. Each retry feeds a soft short for 100 ms

A short below the DRV8874 OCP level (a few amps through long wiring) is only
seen by current. Both the guard and the main loop ignore current for 100 ms
after every power-on, including every retry. With the 40, 80, 160 ms back-off
the first second delivers about half a second of full current. PR #2 lists
this as a known limit.

Fix: skip the inrush grace on a retry that follows a current trip; keep it for
host-commanded power-on only.

### 6. One noisy ADC sample inverts an AUTO track

A current-triggered invert fires on the first tick over the limit, with no
accumulation (`MotorDriver.cpp:329` feeding `TrackManager.cpp:187`). Motor
start spikes or ESP32 ADC noise can flip a loop under a running train. That is
harmless for DCC unless a loco is on the gap, in which case the 20 ms
re-invert lockout leads to a cut. It also silently desyncs the host's belief.

Fix: require two or three consecutive over samples for current-triggered
inverts; keep the fault pin immediate.

### 7. A plain-track fault inverts every AUTO track

With C and D both AUTO, a short seen only by A's driver flips both loops, and
so does any non-loop short on A such as a derailment. If a loco is on the
other loop's gap at that moment it is shorted, locked out for 20 ms, then cut.
Rare.

Fix: invert one AUTO track at a time, starting with the one whose last current
sample was highest, and the other only if the fault persists past the 4 ms
hold.

### 8. The ISR's trip value is a main-loop scratch variable

`tripValue` (`MotorDriver.h:451`) is never initialised and is only refreshed
when `checkPowerOverload` runs for that track in ON or ALERT. It is stale for
a few loop passes after `<JG>` and garbage until the first pass. Low impact
today because the current check is armed 100 ms after power-on.

Fix: set it in the constructor and in `setCurrentLimit`.

### 9. An overload is not a power state the host sees

A fast cut sets OVERLOAD without a `<p>` broadcast, so
`t.reported("power","D")` stays "on" during the back-off. The script's
`<1 D>` logic and any host-side recovery act on wrong state.

Fix: broadcast power on OVERLOAD entry and on RESTORE.

### 10. ISR cost is unmeasured

An ADC conversion with a busy-wait runs every 100 us inside a critical
section, and the main loop's ADC reads block the timer interrupt for the
conversion time.

Fix: count ticks per second and the longest tick; if needed read the ADC on
every second tick.

### 11. The guard interrupt is not IRAM-safe

With no wifi and no EEPROM in this build nothing writes flash, so this is
fine. Worth a comment at the timer setup.

## Recommended order

1. Add a `<D GUARD>` diagnostic printing, per track: mode, `invertPhase`, the
   GPIO invert bit, `guardActive`, `guardAcc`, time since the last invert.
   This makes finding 1 a scripted check over the mirror without a train:
   `<= D INV>`, `<= A MAIN>`, read back, expect D inverted.
2. Fix finding 1 and verify with that check. Record it in an ADR.
3. Findings 2, 3 and 9 together as one change to what the host sees and
   sends; the script depends on all three.
4. Findings 5 and 6 as guard tuning, each checked on the layout with the
   trace build from `v5.6.4-rails49.14-reverse-trace-test` as the harness.
5. Findings 4, 7, 8, 10 as follow-ups.

There is no seam for a regression test on the guard. If this logic keeps
changing, separating the per-tick decision from the register access would let
it compile natively against recorded traces.
