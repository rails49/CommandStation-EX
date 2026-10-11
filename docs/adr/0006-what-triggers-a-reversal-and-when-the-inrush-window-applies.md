# What triggers a Reversal and when the Inrush Window applies

The Guard reversed an AUTO track on the first current sample over its limit,
reversed every AUTO track when only a plain track saw a short, and gave
every power-on an Inrush Window, including each retry after an Overload.

With four tracks sharing the ADC, each track's current is sampled every
400 us.

Decided:

- A current-triggered Reversal needs two consecutive over samples of that
  track. A sample under the limit resets the count. A Fault reverses at once.
- When only a plain track is over, the AUTO track whose last current sample
  was highest reverses. A tie goes to the lower track letter. The other AUTO
  track reverses only if the Fault persists past the 4 ms hold.
- The Inrush Window applies after a host power-on and after a retry that
  follows a Fault. A retry that follows a current Overload gets no window,
  in the Guard and in the main loop alike.
- The Guard's per-tick decision is a plain function of the per-track over
  flags and times, with no register access, so it compiles natively and is
  tested against recorded sample sequences.

Whether the fault pin should be tolerated during the Inrush Window is left
open until a loco with a stay-alive has been measured powering on.

## Considered options

- Three samples for a Reversal. Another 400 us for no clearer signal.
- Keep one sample. One ADC glitch flips a loop under a running train.
- A static map of which AUTO track borders which plain track. On rails49 both
  loops border A, so it decides nothing.
- No window on any retry. A stay-alive that trips the driver at power-on would
  then be cut by current on every retry as well.

## Consequences

- A soft short on an AUTO track reverses about 800 us later than before.
- A retry into a soft short is cut after 1 ms instead of feeding it for
  100 ms.
- A Reversal caused by a plain-track Fault flips one loop, and the other only
  4 ms later if the first was wrong.
