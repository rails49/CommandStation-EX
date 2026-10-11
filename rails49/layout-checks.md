# Layout checks of the Guard

The rules of the Guard are tested natively by `rails49/test-guard.sh`, without
a train. These procedures are for the layout. They confirm on the real station
what the test already says, or, for the stay-alive, measure what nobody knows
yet. Each takes about ten minutes. The diagnostic lines are the record: keep
them with the result.

All of them need a normal build on the station and a terminal on the mirror,
for example `uv run rails49/check.py send "<D GUARD>"`, or any client that
shows the station's `<* ... *>` lines. Words as in `CONTEXT.md`.

## 1. Does a loco with a stay-alive power up on a guarded track?

ADR-0004 took the brake PWM off guarded tracks, and the Guard watches the
fault pin from the first tick of a power-on. Nobody has measured whether a big
stay-alive makes the driver report a Fault at power-on. This decides whether
the fault-pin rule in the Inrush Window has to change (ADR-0006 leaves it open).

1. Put the loco with the largest stay-alive on a guarded MAIN track, say A.
   No other loco on that track.
2. `<0 A>`, wait 10 s so the stay-alive is empty.
3. `<1 A>`, and watch the lines for 10 s.
4. Repeat steps 2 and 3 twice.

Pass: the loco's lights come on and it answers a throttle, and no
`TRACK A FAST OVERLOAD` line appears, on all three power-ons.

Fail: `TRACK A FAST OVERLOAD ... FAULT PIN` right after `<1 A>`, repeated on
each `TRACK A POWER RESTORE`. Then the fault-pin rule in the Inrush Window is
reopened as a new spec, with these lines attached. Nothing else changes it.

## 2. A retry into a soft short is cut after 1 ms

A short below the driver's own trip level is seen only by current. A retry
after such an Overload gets no Inrush Window.

1. Set a guarded track's limit low enough that a resistor exceeds it but the
   driver does not trip, for example `<JG A 500>` and a 10 Ω, 10 W resistor
   across A's rails at 15 V (1.5 A).
2. `<1 A>`, wait 1 s, then connect the resistor. Leave it for 5 s, remove it.
3. Restore A's limit with `<JG A n>`, n from `<JG>` before step 1.

Pass: a `TRACK A FAST OVERLOAD ...mA` line without `FAULT PIN` when the
resistor goes on, and after each `TRACK A POWER RESTORE` an Overload of A
again, with no `TRACK A CURRENT (... ignore)` line between a `POWER RESTORE`
and the Overload that follows it: the retries get no Inrush Window. (Before
the first Overload the main loop may print one; A was not powering on.)
After the resistor is removed, A comes back with `<p1 A>`.

## 3. A short at a loop's gap reverses one loop

A short only the plain track's driver sees reverses the loop with the higher
current, and the other only if the short persists past the 4 ms hold.

1. Two AUTO tracks, as on rails49 C and D, both on and both next to A.
2. Run a train with the wrong Polarity across the gap between A and D.

Pass: one `TRACK D INVERT (fast)` line, no `TRACK C INVERT (fast)` within the
same second, the train runs on, and no `FAST OVERLOAD`. If the line is
`TRACK C INVERT (fast)`, it must be followed within a few ms by
`TRACK D INVERT (fast)`, and still no `FAST OVERLOAD`.
