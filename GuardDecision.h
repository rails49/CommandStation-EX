/*
 *  This file is part of CommandStation-EX
 *
 *  This is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  It is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CommandStation.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef GuardDecision_h
#define GuardDecision_h
#include <stdint.h>

// The Guard's per-tick decision, as a plain function of what the ISR read
// from the drivers. No Arduino or ESP-IDF type may appear here: the native
// test in rails49/ compiles this file with the host compiler.
// See TrackManager::guardTick() for the ISR that feeds it, and ADR-0006.

// Tracks the function handles at most, the station's MAX_TRACKS.
static const uint8_t GUARD_MAX_TRACKS = 8;

// What the ISR read for one track on this tick, and the Guard's state of
// that track as left by the previous tick.
struct GuardIn {
  bool active = false;         // power is on and the Guard watches the track
  bool isAuto = false;         // AUTO track, may be reversed
  bool isProg = false;         // PROG track, its current is left to the main loop
  bool fault = false;          // the fault pin is active
  bool sampled = false;        // the ADC read this track on this tick
  bool currentOver = false;    // the last sample is at or over the trip value
  int lastRaw = 0;             // the last sample, raw counts, offset removed
  unsigned long armAt = 0;     // end of the Inrush Window: current ignored until then
  unsigned long holdUntil = 0; // after a Reversal, nothing is over until then
  bool hasReversed = false;    // lastReversal is valid
  unsigned long lastReversal = 0;
  long acc = 0;                // microseconds over, leaky
  uint8_t overCount = 0;       // consecutive samples over the trip value
};

// What the ISR does to one track after this tick, and the state it keeps
// for the next.
struct GuardOut {
  bool reverse = false;        // flip the Polarity
  bool cut = false;            // cut power, an Overload
  bool hold = false;           // start the settle hold now
  long acc = 0;
  uint8_t overCount = 0;
};

struct GuardTimes {
  long tickUs;                 // time between ticks
  long fastTripUs;             // a track over this long is cut
  unsigned long settleUs;      // the hold after a Reversal
  unsigned long reinvertUs;    // a track is not reversed again within this
};

// Whether a track powering on gets the Inrush Window, ADR-0006: a host
// power-on (from OFF) and a retry after a Fault do, a retry after a current
// Overload does not, so a soft short is cut within the fast trip time.
bool guardInrushWindow(bool fromOverload, bool overloadByCurrent);

// Map the inputs of tracks 0..n-1 to their outputs, n <= GUARD_MAX_TRACKS.
void guardDecide(const GuardIn in[], GuardOut out[], uint8_t n,
                 unsigned long now, const GuardTimes &times);

#endif
