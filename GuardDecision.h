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

// A Reversal by current needs this many consecutive samples over the limit,
// so one ADC glitch or motor start spike does not flip a loop. A Fault
// reverses at once.
static const uint8_t REVERSAL_SAMPLES = 2;

// A track's acc on the tick its REVERSAL_SAMPLES-th consecutive over sample
// is read, with tracks guarded tracks in the ADC round, one read per tick.
// The Reversal is decided before the cut on the same tick, so a track
// reverses and is not cut while this is at most the fast trip time.
constexpr long guardReversalAccUs(long tickUs, uint8_t tracks) {
  return (REVERSAL_SAMPLES - 1) * tracks * tickUs + tickUs;
}

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

// Whether a track powering on gets the Inrush Window, ADR-0006. retry: the
// main loop moves the track from OVERLOAD to ALERT, not the host powering
// it ON. A host power-on, from OFF or OVERLOAD, and a retry after a Fault
// get the window, a retry after a current Overload does not, so a soft
// short is cut within the fast trip time.
bool guardInrushWindow(bool retry, bool overloadByCurrent);

// Map the inputs of tracks 0..n-1 to their outputs, n <= GUARD_MAX_TRACKS.
void guardDecide(const GuardIn in[], GuardOut out[], uint8_t n,
                 unsigned long now, const GuardTimes &times);

#endif
