# Runtime current limits extend `<JG>` rather than taking a new opcode

Each track's current limit was fixed at compile time by the motor shield
definition, adjustable only through the global `MAX_CURRENT` define. To set a
limit per track at runtime we extended the existing `<JG>` gauge query with a
setter form, `<JG track mA>` and `<JG ALL mA>`, instead of claiming a new
opcode. `<JG>` already reports exactly this value, so getter and setter now sit
on one opcode, the read-back format needed no change, and the single-character
opcode table in `DCCEXParser.cpp` needed no new entry — which keeps the change
additive and easy to accept upstream.

## Considered options

- **A free two-letter `J` code** (`JS`, `JN`, …). Cleaner separation of read
  from write, but spends a scarce code and needs upstream to bless it.
- **`<= track LIMIT mA>`.** Reads best in English and joins the other
  track-letter commands, but `<=` means *track mode*, and a current limit is not
  a mode. Widening a command's meaning is a harder sell than extending a getter.
- **`<U ...>` via the weak `myFilter` hook**, the documented user-command
  escape hatch. Conflict-free forever, but no throttle would ever speak it and
  it would mean maintaining a fork.

`<JC>` set the precedent: `<JC>` queries the fast clock and `<JC mins rate>`
sets it. `<JG>` now works the same way.

## Consequences

- Boot behaviour is unchanged. Every track still starts at its shield's limit,
  so an existing installation upgrades with no change in behaviour. Low limits
  at startup are a user's own `mySetup.h` or EXRAIL `PARSE(...)` line, which is
  safe because all tracks boot with power off and nothing energises them during
  `setup()`.
- Limits are volatile. Nothing is written to EEPROM, whose layout is a fixed
  record of turnouts, sensors and outputs with no general settings area.
- The programming track's 250 mA NMRA limit is untouched. A limit set on a track
  in PROG mode is stored but only takes effect where the main trip value already
  applies — when the track leaves PROG mode, and during ack operations, joins
  and `<C PROGBOOST>`, where the prog limit is deliberately bypassed today.
- A limit may be set above the shield's rating. That is the operator's call: the
  intent is protecting wiring and rolling stock from short-circuit currents, not
  protecting the driver chip, which has its own thermal shutdown. The only
  ceiling is the highest current the ADC can report, above which a trip could
  never fire; requests beyond it are silently clamped and `<JG>` reads back the
  value actually adopted.
