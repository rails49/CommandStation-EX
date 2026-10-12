# Polarity is reported from the hardware and set without a power cut

On rails49 the host sets track D's Polarity from the position of turnout 12.
Track D is also AUTO, so the Guard reverses it when a wheel bridges the gap
with the wrong Polarity. Three things kept the host from knowing and setting
the Polarity:

- A Reversal flipped the hardware and left the Track Mode as it was. `<=>`
  reported the mode, so an AUTO track read `MAIN A` whatever its Polarity, and
  a Reversal was announced only as a diagnostic line.
- Every `<=` command that changed the mode word cut the track's power, so a
  change of INV alone stopped a train. No single mode word meant AUTO with
  INV, so setting the Polarity took two commands and up to two cuts.
- An Overload cut by the Guard was not broadcast as a power state. The host
  still read the track as on.

Decided:

- Reports show the Polarity the hardware has. An inverted AUTO track reads
  `MAIN A I`, a plain inverted track `MAIN I`. The mode's INV bit stays what
  the host set.
- The track state line is broadcast after every Reversal, as it is after a
  mode command.
- A mode command cuts power only when the mode without INV and AUTO changes.
  A change of INV or AUTO alone, or a repeat of the same mode, does not.
- `MAIN_AUTO_INV` is a mode word, so one command sets AUTO and the Polarity.
- A track entering Overload broadcasts `<p0 x>`, and `<p1 x>` when it returns
  to on, on every transition.

## Considered options

- A Reversal sets or clears the INV bit in the mode. Then `<= D MAIN_AUTO>`
  means different things depending on history, and the ISR writes the mode
  word the main loop reads.
- A separate line for Polarity next to the mode. One more line to parse for
  no gain.
- No cut only on AUTO tracks. An INV change on a plain MAIN track has the same
  hazard, which is none.

## Consequences

- A client that knows only upstream's mode names sees `MAIN A I`.
- A BOOST track reports its hardware Polarity too, since a Reversal flips its
  output as on MAIN: an inverted BOOST AUTO track reads `BOOST A I` (#31).
- During a persistent short each retry broadcasts one `<p0 x>`, at 40 ms,
  80 ms and so on up to 10 s apart. `<p1 x>` goes out once, when the track
  returns to on.
- The host script sends one of `<= D MAIN_AUTO>` or `<= D MAIN_AUTO_INV>` per
  turnout change and no longer sends `<1 D>` after it.
- `<= D MAIN_AUTO>` on a reversed D resets it to normal Polarity without a
  cut, as before.
