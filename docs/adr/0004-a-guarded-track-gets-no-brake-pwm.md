# A guarded track gets no brake PWM

When the main loop sees a track's fault pin active in ALERT, upstream puts a
62.5 kHz PWM on that track's brake pin (`throttleInrush`). The track brakes for
3 us in every 16 us. The main loop turns it off 10 ms after the fault clears.
Each track turns it on and off on its own, from its own pass of the main loop.

A wheel that bridges the gap between two tracks connects their rails. While
one of them brakes and the other drives, that is a short.

On rails49, track D is a reverse loop next to A. When a wheel crosses the gap
with the wrong polarity, both DRV8874 drivers switch off and pull their fault
pins low. The fast overcurrent guard inverts D. A trace of the fault pins and
the brake PWM after each invert (#7) showed:

- The brake PWM on A and D started 1.5-2.7 ms after the short, about 1 ms
  apart.
- The drivers retry after 2.0 ms. The fault came back 0.1 ms later, while only
  one of the two tracks had the brake PWM on.
- With both on, the next retry held.
- At about 14 ms one track turned its PWM off, and the fault came back within
  0.1 ms. This repeated every 12 ms while the wheel bridged the gap.
- The guard ignores overcurrent for 4 ms after an invert. A fault after that
  is cut after 1 ms, so A and D lost power for 40 ms. The loco stopped.

5 of 11 reversals were cut. In every trace, each fault after the first started
within 0.1 ms of a moment when only one track had the brake PWM on.

A track the guard watches (`guardActive`) gets no brake PWM.

## Consequences

- With this, 10 of 10 reversals were not cut. A fault after an invert cleared
  at the driver's first retry, 1.9-2.1 ms.
- On a guarded track the guard cuts a fault after 1 ms, before the main loop
  would start the brake PWM. A short is handled as before.
- A loco whose capacitor charge trips the driver at power on gets no throttled
  charging on a guarded track. The guard cut it after 1 ms before this change
  as well.
- Tracks the guard does not watch keep the brake PWM: DC tracks, tracks with
  neither a fault pin nor current sense, and every track on a station that is
  not an ESP32.
- Starting and stopping the brake PWM on all tracks together was not done. The
  station does not know which tracks share a gap, so a short on one track
  would throttle all of them.
