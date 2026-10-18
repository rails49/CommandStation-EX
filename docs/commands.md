# Command reference, rails49 build

The text commands this station takes, for anyone writing a client: the dccex
translator, its railroad script, a console session. It covers the commands
those clients use and every command this fork added or changed, as the
current source behaves. Everything else upstream has is left out; see
[Upstream commands](#upstream-commands).

Words used here are defined in [CONTEXT.md](../CONTEXT.md). The dccex side
calls a Track a "district" and an Overload a "trip".

## Rules for every command

**Form.** A command is `<`, a one-character opcode, parameters separated by
spaces, `>`. The opcode is case-sensitive. A keyword may follow the opcode
without a space: `<JG>` is opcode `J` with the keyword `G`, `<!Q>` is opcode
`!` with `Q`.

**Parameters.** A parameter is a decimal integer from -32768 to 32767 or a
keyword. Keywords are upper-cased and turned into a number before the command
sees them, so a keyword and a number can be equal: a single letter equals its
ASCII code, and `<1 65>` is `<1 A>`. A command takes at most 9 parameters.
Every command below checks its parameter count.

**Tracks.** A track is named by a letter. This build has A, B, C and D. Lists
of per-track values are in that order.

**What comes back.** Every line the station sends ends in a newline and is
one of:

- a **Reply**, sent only to the client whose command caused it;
- a **Broadcast**, sent to every client;
- a **Diagnostic**, `<* … *>`, sent on USB serial only.

A Reply of `<X>` means the command was refused and changed nothing. Many
commands that succeed send no Reply; their effect shows as a Broadcast or not
at all. Each entry below says which.

**One client on rails49.** This build has no wifi, so the station has one
client: the USB link. The dccex mirror passes every line to every TCP client
on port 2560. Each client therefore sees every Reply, Broadcast and
Diagnostic, including those caused by other clients' commands. Match a line
to a command by its content, not by who received it.

**Typos.** Most opcodes are upstream commands, so a mistyped opcode can run a
different command rather than get `<X>`. `<zQ>`, for example, sets vpin 81
high.

**Examples.** In the example exchanges, `→` marks a line the client sends
and `←` a line the station sends back.

**Lines to ignore.** `<@ screen row "text">` copies a line of the station's
display. Lines not described here come from upstream features.

## Track power and modes

### Power on and off: `<1>`, `<0>`, `<1 X>`, `<0 X>`

| Command | Effect |
|---|---|
| `<1>` | Power on every track. |
| `<0>` | Power off every track. |
| `<1 X>` | Power on track X. |
| `<0 X>` | Power off track X. |

- **Parameters:** X is a track letter.
- **Reply:** none.
- **Broadcast:** the [power lines](#power-lines), if any track's power
  changed.
- **Error:** `<X>` for more than one parameter or a letter outside A–H. A
  letter of a track this build does not have (E–H) gets the Diagnostic
  `<* Error: Track F does not exist *>` and no `<X>`.
- **Notes:** A track in NONE mode stays off. A track that powers on has
  100 ms (the Inrush Window) in which current over its limit is tolerated.
  A mode change can turn a track's power off; see `<= X MODE>`.

```
→ <1 D>
← <p0 A>
← <p0 B>
← <p0 C>
← <p1 D>
```

### Power lines

The block of lines that reports power, Broadcast after every power change and
in answer to `<s>`:

1. `<p1 X>` or `<p0 X>` for each track. `1` means the track is on. `0` means
   off or in Overload.
2. `<p1>` if every track is on. `<p0>` if no track is on, or every track not
   in NONE mode is off. Otherwise no line.
3. `<p1 MAIN>` if at least one MAIN track is on and none is off. `<p1 PROG>`
   if the PROG track is on.

Read the per-track lines; lines 2 and 3 are summaries of them.

### Set a track mode: `<= X MODE>`

| MODE (sent) | Track carries | Reported as |
|---|---|---|
| `MAIN` | DCC, normal Polarity | `MAIN` |
| `MAIN_INV` | DCC, inverted Polarity | `MAIN I` |
| `MAIN_AUTO` | DCC, normal Polarity, Reversal on a short | `MAIN A` |
| `MAIN_AUTO_INV` | DCC, inverted Polarity, Reversal on a short | `MAIN A I` |
| `PROG` | the Programming Track | `PROG` |
| `NONE` (or `OFF`) | nothing; power stays off | `NONE` |

- **Sent and reported names:** `MAIN_INV` and `MAIN I` are the same mode.
  The station takes only the name in the MODE column and reports only the
  one in the "Reported as" column; `<= X MAIN I>` gets `<X>`.
- **Parameters:** X is a track letter. The track must exist.
- **Power:** a change between MAIN, PROG and NONE turns the track's power
  off; send `<1 X>` to turn it back on. A change among the four MAIN modes,
  or a repeat of the current mode, keeps power and signal on.
  `<= X MAIN_AUTO>` on a reversed track sets normal Polarity without a cut.
- **One Programming Track:** setting PROG on a track moves the previous PROG
  track to NONE.
- **Reply:** none.
- **Broadcast:** `<= X NAME>` (the "Reported as" column), then the power
  lines. A track that lost PROG gets `<= Y NONE>`. When a track enters or
  leaves PROG, `<jG …>` follows, since its Current Limit can change with it.
- **Error:** `<X>` for an unknown mode, a letter outside A–H, or a track
  that does not exist.

```
→ <= D MAIN_AUTO_INV>
← <= D MAIN A I>
← <p0 A>
← <p0 B>
← <p0 C>
← <p1 D>
```

### Set a Current Limit: `<JG X mA>`, `<JG ALL mA>`

- **Parameters:** X is a track letter, or `ALL` for every track. mA is
  1–32767. A value above what the track can measure is lowered to that
  maximum, with no `<X>`; `<jG>` shows the value in force.
- **Effect:** the limit is the one current the track overloads at, in every
  mode, from the Guard's next check. On a PROG track it replaces the 250 mA
  Prog Limit. It is lost when the station restarts.
- **Reply:** none.
- **Broadcast:** `<jG A B C D>`, every track's Current Limit in mA.
- **Error:** `<X>` for mA ≤ 0, a letter outside A–H, or a track that does not
  exist or cannot measure current. `<JG ALL mA>` gets `<X>` only if no track
  took the limit.

```
→ <JG C 1500>
← <jG 2000 250 1500 500>
→ <JG ALL 1000>
← <jG 1000 1000 1000 1000>
```

`<JG ALL mA>` sets the PROG track too.

### When a track overloads

A track overloads when its current reaches its Current Limit or its motor
driver reports a Fault. Its power is cut and retried, first after 40 ms, then
after twice the previous wait, up to 10 s.

1. The cut Broadcasts the power lines, with `<p0 X>`.
2. Each retry that fails Broadcasts them again, with `<p0 X>`.
3. Once a retry holds for 20 ms, the power lines are Broadcast with `<p1 X>`.

The Diagnostics for these steps are listed under [Diagnostics](#diagnostics).

### Reversal

On an AUTO track a short flips the track's Polarity instead of cutting power.
The station Broadcasts `<= X MAIN A I>` (or `<= X MAIN A>` when it flips
back) and the power lines, then sends the Diagnostic `TRACK X INVERT (fast)`.
A short that the Reversal does not clear becomes an Overload. What triggers a
Reversal is in ADR-0006.

## Locos

A loco is named by its DCC address, the cab: 1–10239. The first command for a
cab takes a slot in the station's loco table; the station repeats the speed
and functions of every loco in the table.

### Speed and direction: `<t cab speed dir>`

- **Parameters:**
  - cab: 1–10239.
  - speed: 0 is stop, 1–126 are the 126 speed steps, -1 is an emergency stop
    of this loco.
  - dir: 1 forward, 0 reverse.
- **Reply:** none.
- **Broadcast:** `<l …>` for the loco, if its speed or direction changed.
- **Error:** `<X>` for a value out of range or a full loco table.
- **Notes:** with four parameters the first is ignored and a `<T …>` Reply is
  added. That form is deprecated; send three.

```
→ <t 3 25 1>
← <l 3 0 154 0>
```

### Ask for a loco's state: `<t cab>`

- **Reply:** `<l cab -1 128 0>` if the cab has no slot.
- **Broadcast:** `<l …>` for the loco if it has a slot.
- **Error:** `<X>` for a cab outside 0–10239.

### The loco line: `<l cab 0 speedbyte functions>`

Broadcast whenever a loco's speed, direction or functions F0–F31 change.

- **speedbyte:** bit 7 is the direction, 1 forward. The low 7 bits are 0 for
  stop, 1 for emergency stop, and n for speed step n-1. `154` is forward,
  step 25.
- **functions:** bit n is Fn, as a decimal number. With F31 on the number is
  negative.
- **Notes:** the speed is the one last commanded, not the one the loco has
  reached. The second field is always 0, except `-1` in a Reply for a cab with
  no slot.

### Functions: `<F cab fn state>`

- **Parameters:**
  - cab: 1–10239.
  - fn: 0–28 are the loco's functions. 29 and above are sent as binary state
    packets.
  - state: 1 on, 0 off.
- **Reply:** none.
- **Broadcast:** `<l …>` for the loco, if F0–F31 changed.
- **Error:** `<X>` for cab ≤ 0, fn < 0 or a full loco table.

```
→ <F 3 0 1>
← <l 3 0 154 1>
```

### Emergency stop: `<!>`

- **Effect:** stops every loco now. Each keeps its direction and stays at
  emergency stop until its next `<t>`.
- **Reply:** none.
- **Broadcast:** `<l cab 0 1 fn>` or `<l cab 0 129 fn>` for each loco in the
  table.
- **Error:** none. A parameter other than P, R or Q acts as `<!>`. `<!P>`,
  `<!R>` and `<!Q>` are upstream's stop lock; the translator sends none of
  them.

## Accessories

### Points and other basic accessories: `<a addr sub act>`

- **Parameters:**
  - addr: decoder address, 0–511.
  - sub: output on that decoder, 0–3.
  - act: which of the output's two coils, 0 or 1.
- **Effect:** the station sends the "on" packet, then the "off" packet 100 ms
  later. It keeps no record of the accessory's state.
- **Reply:** none.
- **Broadcast:** none.
- **Error:** `<X>` for a value out of range.
- **The form depends on the parameter count.** With two parameters,
  `<a n act>`, n is a linear address: addr = (n-1)/4 + 1, sub = (n-1) mod 4.
  `<a 5 1>` is linear address 5, never addr 5 sub 1.
- **Notes:** the translator turns its linear point number L (1–2044) into
  `<a (L-1)/4+1 (L-1)%4 act>`, with act 1 for thrown.

```
→ <a 3 2 1>
```

### Signals and other extended accessories: `<A addr value>`

- **Parameters:**
  - addr: -3–2044, the address a user sees. The packet goes to addr+3, as
    RCN-213 numbers them.
  - value: 0–31. 0 is the stop aspect; the decoder defines the others.
- **Reply:** none.
- **Broadcast:** none.
- **Error:** `<X>` for a value out of range or a parameter count other than 2.

```
→ <A 5 2>
```

## Checking the station

### Status: `<s>`

1. **Reply:** `<iDCC-EX V-5.6.3 / ESP32 / EXCSB1_WITH_EX8874 G-tag>`. The
   tag after `G-` names the release on the station.
2. **Broadcast:** the power lines.
3. **Reply:** `<H id 0|1>` for each turnout and `<Q id>` or `<q id>` for each
   sensor. This build defines neither, so none are sent.

```
→ <s>
← <iDCC-EX V-5.6.3 / ESP32 / EXCSB1_WITH_EX8874 G-v5.6.3-rails49.19>
← <p0 A>
← <p0 B>
← <p0 C>
← <p0 D>
← <p0>
```

### Track modes: `<=>`

- **Reply:** `<= X NAME>` for each track, with the names from
  [`<= X MODE>`](#set-a-track-mode--x-mode). `I` shows the Polarity the track
  has now, so a reversed AUTO track reads `MAIN A I`.

```
→ <=>
← <= A MAIN>
← <= B PROG>
← <= C MAIN A>
← <= D MAIN A>
```

### Current Limits: `<JG>`

- **Reply:** `<jG A B C D>`, each track's Current Limit in mA, as set by
  `<JG X mA>`. A track never given one reports 250 in PROG mode and its Board
  Limit, 5000, otherwise.
- **Error:** `<X>` for two parameters or more than three.

```
→ <JG>
← <jG 2000 250 1500 500>
```

### Track currents: `<JI>`

- **Reply:** `<jI A B C D>`, each track's current in mA: the mean of the
  samples taken in the last complete 250 ms window. A read does not reset
  it, so any number of clients may poll `<JI>` and read the same value at
  the same moment; it changes once per window. Within 250 ms of a track
  powering on, the mean of the samples so far. `-1` for a track in Overload.

```
→ <JI>
← <jI 112 0 0 37>
```

### Restart check: `<JM id>`, `<JM id value>`

A stash is a value the station holds in RAM by id. It is empty after every
restart.

- `<JM id value>` sets it. Reply: none. `0` clears it.
- `<JM id>` reads it. Reply: `<jM id value>`, `0` if unset.

The translator sets `<JM 32000 1>` when it connects and polls `<JM 32000>`
every second. `<jM 32000 0>` means the station restarted.

```
→ <JM 32000>
← <jM 32000 1>
```

### Host Timeout: `<G n>`

- **Parameters:** n is 0–3600 s. `<G 0>` turns the Host Timeout off. It is
  off after a restart.
- **Effect:** if no further `<G n>` arrives within n seconds, the station
  stops every loco as `<!>` does, turns every track off, and sends the
  Diagnostic `HOST TIMEOUT: TRACK POWER OFF`. The Host Timeout is then off
  until the next `<G n>`.
- **Reply:** none.
- **Broadcast:** none, until it fires; then the `<l …>` lines and the power
  lines.
- **Error:** `<X>` for n outside 0–3600 or a parameter count other than 1.
- **Notes:** the translator sends `<G 3>` each time `layout` confirms it is
  alive.

### The Guard: `<D GUARD>`

- **Reply:** none. The answer is Diagnostics:
  - one line per track,
    `GUARD X MODE inv=0|1 gpio=0|1 active=0|1 acc=Nus last=Nms|- trip=raw/mA`;
  - then `GUARD ticks=N/s max=Nus`.
- **Fields:**
  - MODE is the mode the host set; a Reversal does not change it.
  - inv is the Polarity the station holds; gpio is the invert bit read back
    from the hardware.
  - active is 1 while the Guard watches the track. It is 0 while the track
    is off, and on a track the Guard cannot watch.
  - acc is the time over the limit counted so far.
  - last is the time since the Guard's last Reversal of this track, `-` if it
    never reversed it.
  - trip is the Current Limit in ADC counts and mA.
  - ticks and max are the Guard's checks per second and its longest check,
    both since the last `<D GUARD>`.
- **Error:** `<X>` on builds without the Guard, and on rails49 builds up to
  `v5.6.4-rails49.16`.

```
→ <D GUARD>
← <* GUARD A MAIN inv=0 gpio=0 active=0 acc=0us last=- trip=898/1999mA *>
← <* GUARD B PROG inv=0 gpio=0 active=0 acc=0us last=- trip=112/249mA *>
← <* GUARD C MAIN A inv=0 gpio=0 active=0 acc=0us last=- trip=987/1499mA *>
← <* GUARD D MAIN A inv=0 gpio=0 active=0 acc=0us last=- trip=329/499mA *>
← <* GUARD ticks=9973/s max=8us *>
```

### Diagnostics

Diagnostics the station sends on its own. The power lines and track state
lines carry the same events and are the ones to parse; the Diagnostic text can
change.

| Diagnostic | When |
|---|---|
| `TRACK X ALERT nmA`, `TRACK X ALERT FAULT nmA`, `TRACK X ALERT FAULT` | the main loop saw current over the limit or a Fault |
| `TRACK X FAST OVERLOAD nmA[ FAULT PIN]. Pause t` | the Guard cut the track |
| `TRACK X POWER OVERLOAD nmA (max nmA) detected after t. Pause t` | the main loop cut the track for current |
| `TRACK X FAULT PIN detected after t. Pause t)` | the main loop cut the track for a Fault |
| `TRACK X POWER RESTORE (after t)` | a retry after an Overload |
| `TRACK X NORMAL (after t/t) nmA` | the track is back on |
| `TRACK X INVERT (fast)` | the Guard reversed the track |
| `TRACK X INVERT` | the main loop reversed a track the Guard does not watch |
| `HOST TIMEOUT: TRACK POWER OFF` | the Host Timeout fired |
| `Error: Track X does not exist` | `<1 X>` or `<0 X>` named a missing track |

Times t carry their unit: `usec`, `msec` or `sec`.

## Upstream commands

Everything not described here, including EXRAIL, turnout and sensor
definitions, programming-track commands, DC modes and consists, is in the
upstream [DCC-EX command summary](https://dcc-ex.com/reference/software/command-summary-consolidated.html).
