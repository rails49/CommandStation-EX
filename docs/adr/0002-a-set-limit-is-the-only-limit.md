# A limit set with `<JG>` is a track's only limit

Supersedes the third consequence of ADR-0001.

Before `<JG>`, a track in PROG mode had two thresholds: the Prog Limit (250
mA) and its Board Limit, used during ack operations, joins and `<C
PROGBOOST>`. `<JG>` reported the Board Limit, a value that does not apply while
the track is in PROG mode. ADR-0001 kept that: a limit set on a PROG track was
stored and applied only where the Board Limit already did.

A track now has one Current Limit, and `<JG>` reports it.

- A track whose limit was never set behaves as upstream does: the Prog Limit in
  PROG mode, with its bypasses, and the Board Limit otherwise. `<JG>` reports
  the Prog Limit for such a track while it is in PROG mode.
- `<JG track mA>` sets the one limit the track trips at, in every mode and
  state. The Prog Limit and its bypasses no longer apply to that track.
- A mode change into or out of PROG broadcasts `<jG>`, since the reported limit
  can change with it.
- `<JG>` reports the limit in mA as it was given, not converted to an ADC
  count and back, so a limit set to 250 reads 250 rather than 249. A limit
  clamped to the ADC's range reads as the clamped value.

## Consequences

- Boot behaviour is unchanged. Only `<JG>`'s answer for a PROG track differs
  from upstream: the Prog Limit instead of the Board Limit.
- During an ack operation on a PROG track whose limit was never set, `<JG>`
  still reports the Prog Limit while the Board Limit briefly applies.
- A PROG track given a limit with `<JG>` keeps it through ack operations. A
  decoder whose ack draws more than that limit reads back as no ack.
