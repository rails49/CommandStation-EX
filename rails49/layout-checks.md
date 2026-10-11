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

## 2. A retry into a soft short is cut within a few ms

A short below the driver's own trip level is seen only by current. A host
power-on gets the Inrush Window, a retry after such an Overload does not
(ADR-0006, #20). Needs the user's OK to power the track.

1. Note C's limit from `<JG>`. Set it low enough that a resistor exceeds it
   but the driver does not trip, for example `<JG C 500>` and a 10 Ω, 10 W
   resistor at 15 V (1.5 A).
2. `<0 C>`, then put the resistor across C's rails.
3. Start a client collecting lines, then `<1 C>`. Leave it for 5 s, `<0 C>`,
   remove the resistor.
4. Restore C's limit with `<JG C n>`, n from step 1.

Pass: the first `TRACK C FAST OVERLOAD ...mA` line, without `FAULT PIN`,
comes about 100 ms after `<1 C>`, after the window: the main loop may print
`TRACK C CURRENT (100ms ignore)` before it. After each
`TRACK C POWER RESTORE` the next `TRACK C FAST OVERLOAD` follows within a few
ms, with no `TRACK C CURRENT (... ignore)` line between them. C is AUTO, so
a `TRACK C INVERT (fast)` line before a cut is expected: the Reversal does
not help a resistor. A `FAULT PIN` on the cut means the resistor trips the
driver; use a larger one.

The lines go into the PR with the resistor, the voltage and the limit used.

## 3. A short at a loop's gap reverses one loop

A short only the plain track's driver sees reverses the loop with the higher
current, and the other only if the short persists past the 4 ms hold.

1. Two AUTO tracks, as on rails49 C and D, both on and both next to A.
2. Run a train with the wrong Polarity across the gap between A and D.

Pass: one `TRACK D INVERT (fast)` line, no `TRACK C INVERT (fast)` within the
same second, the train runs on, and no `FAST OVERLOAD`. If the line is
`TRACK C INVERT (fast)`, it must be followed within a few ms by
`TRACK D INVERT (fast)`, and still no `FAST OVERLOAD`.

## 4. An Overload reads as off to the host, and as on when it is back

A track the station cut for an Overload broadcasts `<p0 x>` on every entry
to OVERLOAD, from the Guard or from the main loop, and `<p1 x>` when it is
back on (ADR-0005, #16). Needs the user's OK to power the track.

1. No train on D. `<= D MAIN_AUTO>`, then start
   `uv run rails49/check.py overload-broadcast --wait 15`, which sends `<1 D>`.
2. When it asks, hold a wire across D's rails for about a second, then
   remove it.

Pass: the check says `passed`. The lines show a `TRACK D INVERT (fast)` (D is
AUTO; the Reversal does not help a wire), then `TRACK D FAST OVERLOAD` and
`<p0 D>` before the first `TRACK D POWER RESTORE`, another `<p0 D>` after
each retry the wire was still there, and after the wire is gone
`TRACK D NORMAL` and `<p1 D>`. A retry that comes back clean prints no
power line of its own, only the `<p1 D>` once it is back on.

Fail: no `<p0 D>` after an OVERLOAD line, or no `<p1 D>` at the end.

The lines go into the PR.
