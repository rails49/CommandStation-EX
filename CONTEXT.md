# CommandStation-EX

A DCC++EX command station: it generates DCC track signals, drives one or more
motor drivers, and accepts text commands from throttles, JMRI and serial hosts.

This glossary fixes the vocabulary used when discussing the station's track
power and current protection. It is a glossary only — no implementation notes,
no decisions.

## Language

### Track power

**Track**:
One independently powered and independently controlled output of the station,
identified by a letter A–H and backed by exactly one motor driver. Up to eight
exist.
_Avoid_: power zone, power district, section, block, channel

**Track Mode**:
What a track's output currently carries: MAIN, PROG, DC, DCX, EXT, BOOST or
NONE, optionally modified by INV or AUTO. A property of the track, changed at
runtime.
_Avoid_: track type, track setting

**Programming Track**:
The single track in PROG mode. At most one track may be in PROG mode at a time.
_Avoid_: service mode track, prog track

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

**Overload**:
The state a track enters when its current reaches its Current Limit. Power is
cut and re-tried on a lengthening back-off.
_Avoid_: trip, fault, short

**Fault**:
A condition the motor driver hardware itself reports on its fault pin, distinct
from an Overload detected by measuring current.
_Avoid_: error, overload
