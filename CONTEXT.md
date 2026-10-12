# CommandStation-EX

A DCC++EX command station: it generates DCC track signals, drives one or more
motor drivers, and accepts text commands from throttles, JMRI and serial hosts.

This glossary fixes the vocabulary used when discussing the station's track
power, current protection and command protocol. It is a glossary only — no implementation notes,
no decisions.

## Language

### Track power

**Track**:
One independently powered and independently controlled output of the station,
identified by a letter A–H and backed by exactly one motor driver. Up to eight
exist.
_Avoid_: power zone, power district, district, section, block, channel

**Track Mode**:
What a track's output currently carries: MAIN, PROG, DC, DCX, EXT, BOOST or
NONE, optionally modified by INV or AUTO. A property of the track, changed at
runtime.
_Avoid_: track type, track setting

**Programming Track**:
The single track in PROG mode. At most one track may be in PROG mode at a time.
_Avoid_: service mode track, prog track

**Polarity**:
Which rail of a track carries which half of the DCC signal: normal or
inverted. Set by the host with INV, or flipped by the station on an AUTO
track.
_Avoid_: phase, inversion

**Reversal**:
The station flipping an AUTO track's Polarity on its own after a short.
_Avoid_: auto-invert, flip, auto-reverse

### Current protection

**Current Limit**:
The current above which a track's power is cut. A property of a single track,
expressed in milliamps.
_Avoid_: max current, trip current, current threshold, gauge, ampere limit

**Board Limit**:
The current limit a motor driver is constructed with, stated by the board
definition for the shield in use. The station's starting value for a track's
Current Limit.
_Avoid_: hardware limit, default limit, shield limit

**Prog Limit**:
250 mA, the starting Current Limit of a track in PROG mode. It is bypassed
during ack operations, joins and `<C PROGBOOST>`. A Current Limit set with
`<JG>` replaces it.
_Avoid_: NMRA limit, service mode limit

**Overload**:
The state a track enters when its current reaches its Current Limit. Power is
cut and re-tried on a lengthening back-off.
_Avoid_: trip, fault, short

**Fault**:
A condition the motor driver hardware itself reports on its fault pin, distinct
from an Overload detected by measuring current.
_Avoid_: error, overload

**Guard**:
The station's fast check of every track's fault pin and current, which cuts
power or triggers a Reversal within about a millisecond.
_Avoid_: fast overcurrent guard, ISR

**Inrush Window**:
The period after a track powers on in which current over the limit is
tolerated so decoders can charge.
_Avoid_: grace period, inrush grace

### Command protocol

**Reply**:
A line the station sends only to the client whose command caused it.
_Avoid_: response, answer, echo

**Broadcast**:
A line the station sends to every connected command client, the client whose
command caused it included.
_Avoid_: notification, event, push

**Diagnostic**:
A `<* … *>` line, sent only on USB serial.
_Avoid_: log, debug message, DIAG

**Host Timeout**:
A one-shot timer the host arms with `<G n>`. If no further `<G n>` arrives
within n seconds, the station stops every loco and turns all track power off.
_Avoid_: watchdog, keepalive, heartbeat
