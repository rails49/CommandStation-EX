# CommandStation-EX

A DCC++EX command station: it generates DCC track signals, drives one or more
motor drivers, and accepts text commands from throttles, JMRI and serial hosts.

This glossary fixes the vocabulary used when discussing the station's track
power, current protection and command protocol. Every entry links to the doc
that owns it (org
[ADR-0017](https://github.com/rails49/.github/blob/main/docs/adr/0017-each-fact-has-one-owner-and-an-accepted-adr-does-not-change.md)).

## Language

### Track power

**Track**:
One independently powered and independently controlled output of the station,
identified by a letter A–H and backed by exactly one motor driver. Up to eight
exist
([docs/commands.md](docs/commands.md#rules-for-every-command)).
_Avoid_: power zone, power district, district, section, block, channel

**Track Mode**:
What a track's output currently carries: MAIN, PROG, DC, DCX, EXT, BOOST or
NONE, optionally modified by INV or AUTO. A property of the track, changed at
runtime
([docs/commands.md](docs/commands.md#set-a-track-mode--x-mode)).
_Avoid_: track type, track setting

**Programming Track**:
The single track in PROG mode
([docs/commands.md](docs/commands.md#set-a-track-mode--x-mode)).
_Avoid_: service mode track, prog track

**Join**:
The station state, set with `<1 JOIN>`, in which the Programming Track carries
the same DCC signal as the MAIN tracks, so a loco on it can drive onto the
layout. A state of the station, not a Track Mode
([docs/commands.md](docs/commands.md#join-1-join)).
_Avoid_: JOIN mode, joined tracks, sync main

**Polarity**:
Which rail of a track carries which half of the DCC signal: normal or
inverted
([docs/commands.md](docs/commands.md#set-a-track-mode--x-mode)).
_Avoid_: phase, inversion

**Reversal**:
The station flipping an AUTO track's Polarity on its own after a short
([docs/commands.md](docs/commands.md#reversal)).
_Avoid_: auto-invert, flip, auto-reverse

### Current protection

**Current Limit**:
The current above which a track's power is cut. A property of a single track,
expressed in milliamps
([docs/commands.md](docs/commands.md#set-a-current-limit-jg-x-ma-jg-all-ma)).
_Avoid_: max current, trip current, current threshold, gauge, ampere limit

**Board Limit**:
The current limit a motor driver is constructed with, stated by the board
definition for the shield in use. The station's starting value for a track's
Current Limit
([docs/commands.md](docs/commands.md#current-limits-jg)).
_Avoid_: hardware limit, default limit, shield limit

**Prog Limit**:
250 mA, the starting Current Limit of a track in PROG mode
([ADR-0002](docs/adr/0002-a-set-limit-is-the-only-limit.md)).
_Avoid_: NMRA limit, service mode limit

**Overload**:
The state a track enters when its current reaches its Current Limit or its
motor driver reports a Fault. Its power is cut
([docs/commands.md](docs/commands.md#when-a-track-overloads)).
_Avoid_: trip, fault, short

**Fault**:
A condition the motor driver hardware itself reports on its fault pin, as
opposed to current the station measures. A cause of an Overload, not the
Overload itself
([docs/commands.md](docs/commands.md#when-a-track-overloads)).
_Avoid_: error, overload

**Guard**:
The station's fast check of every track's fault pin and current, which cuts
power or triggers a Reversal
([docs/commands.md](docs/commands.md#the-guard-d-guard)).
_Avoid_: fast overcurrent guard, ISR

**Inrush Window**:
The period after a track powers on in which current over the limit is
tolerated so decoders can charge
([ADR-0006](docs/adr/0006-what-triggers-a-reversal-and-when-the-inrush-window-applies.md)).
_Avoid_: grace period, inrush grace

### Command protocol

**Reply**:
A line the station sends only to the client whose command caused it
([docs/commands.md](docs/commands.md#rules-for-every-command)).
_Avoid_: response, answer, echo

**Broadcast**:
A line the station sends to every connected command client, the client whose
command caused it included
([docs/commands.md](docs/commands.md#rules-for-every-command)).
_Avoid_: notification, event, push

**Diagnostic**:
A `<* … *>` line, sent only on USB serial
([docs/commands.md](docs/commands.md#rules-for-every-command)).
_Avoid_: log, debug message, DIAG

**Host Timeout**:
A one-shot timer the host arms with `<G n>` that turns all track power off
when it runs out
([docs/commands.md](docs/commands.md#host-timeout-g-n)).
_Avoid_: watchdog, keepalive, heartbeat
