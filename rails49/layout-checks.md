# Layout checks of the Guard

The rules of the Guard are tested natively by `rails49/test-guard.sh`, without
a train. These procedures are for the layout. They confirm on the real station
what the test already says, or, for the stay-alive, measure what nobody knows
yet. Each takes about ten minutes. The diagnostic lines are the record: keep
them with the result.

All of them need a normal build on the station and a terminal on the mirror,
for example `uv run rails49/check.py send "<D GUARD>"`, or any client that
shows the station's `<* ... *>` lines. Words as in `GLOSSARY.md`.

## 1. Does a loco with a stay-alive power up on a guarded track?

ADR-0004 took the brake PWM off guarded tracks, and the Guard watches the
fault pin from the first tick of a power-on. Nobody has measured whether a big
stay-alive makes the driver report a Fault at power-on. This decides whether
the fault-pin rule in the Inrush Window has to change (ADR-0006 leaves it open).

Only a loco with a large stay-alive answers the question. Without one, record
that and stop; run the check again when such a loco joins the layout.

1. Turn control's power off (`power_wanted` `off`) and start a client that
   keeps every line with a time.
2. Put the loco with the largest stay-alive on A, a plain MAIN track. No other
   loco on that track. F0 on, so its lights show when the stay-alive is empty.
3. `<0 A>`. Wait until the lights are out, then 10 s more, at least 30 s in
   all.
4. `<1 A>`, and watch the lines for 10 s. The loco answers the throttle if it
   moves a few cm forward and back at a low speed step within 5 s.
5. Repeat steps 3 and 4 twice.
6. Read `<JG>`. With A and the AUTO track with the lower limit both on, drive
   the loco onto that track and stop it. Then steps 3 to 5 on that track.
7. Once, with the loco on A: power on from the control app's button, which
   sends `<1>` to all tracks. The panel shows no fault.

Pass: on every power-on the lights come on, the loco answers the throttle,
and no `FAST OVERLOAD` line appears for its track.

Fail: the loco never powers up, or every power-on of a track gives
`TRACK X FAST OVERLOAD ... FAULT PIN` before a `TRACK X POWER RESTORE`, even
when the retry comes back `NORMAL`. On the AUTO track a Fault reverses first,
so the lines start with `TRACK X INVERT (fast)`. A single such cut is noted
and the power-on repeated. On a fail the fault-pin rule in the Inrush Window
is reopened as a new spec, with these lines attached. Nothing else changes it.

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

## 5. A Reversal tells the host the new Polarity

When the Guard reverses an AUTO track, the station broadcasts the track's
state line, named from the hardware invert flag, and then the Reversal's
diagnostic (ADR-0005, #15). Needs the user's OK to power D.

1. Without a train: `uv run rails49/check.py reports`. Pass: it says
   `passed`; `<=>` read `<= D MAIN A I>` after `<= D INV>` and `<= D MAIN A>`
   after `<= D MAIN_AUTO>`.
2. A loco next to the gap at the end of D where D's normal Polarity is the
   wrong one; the check sets D to `MAIN_AUTO`, normal Polarity.
3. Start `uv run rails49/check.py reversal-broadcast --wait 30`, which sends
   `<= D MAIN_AUTO>` and `<1 D>`.
4. When it asks, run the loco across the gap into D.

Pass: the check says `passed`, the loco runs on, and the lines show

    <= D MAIN A I>
    <p1 A>
    ...
    <* TRACK D INVERT (fast) *>

If the loco then leaves D across its other gap, that Reversal gives
`<= D MAIN A>` the same way. Fail: a `TRACK D INVERT` line with no
`<= D ...>` line before it, back to the previous `<* ... *>` line.

The lines of steps 1 and 4 go into the PR.
