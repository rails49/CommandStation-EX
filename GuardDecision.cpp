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
#include "GuardDecision.h"

// Runs in the Guard's interrupt on ESP32, so it goes to IRAM there.
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_attr.h>
#define GUARD_DECISION_ATTR IRAM_ATTR
#else
#define GUARD_DECISION_ATTR
#endif

static inline bool GUARD_DECISION_ATTR held(const GuardIn &in, unsigned long now) {
  return (long)(now - in.holdUntil) < 0;
}

// A track that is not locked out by its last Reversal.
static inline bool GUARD_DECISION_ATTR canReverse(const GuardIn &in, unsigned long now,
                                                  const GuardTimes &times) {
  return in.active && in.isAuto
    && !(in.hasReversed && now - in.lastReversal < times.reinvertUs);
}

bool guardInrushWindow(bool retry, bool overloadByCurrent) {
  return !(retry && overloadByCurrent);
}

void GUARD_DECISION_ATTR guardDecide(const GuardIn in[], GuardOut out[], uint8_t n,
                                     unsigned long now, const GuardTimes &times) {
  bool over[GUARD_MAX_TRACKS];    // counts toward the cut
  bool trigger[GUARD_MAX_TRACKS]; // is a short, may cause a Reversal
  bool anyOver = false;
  bool autoTrigger = false;
  bool mainTrigger = false; // a track that can border a reverse loop, not PROG

  for (uint8_t t = 0; t < n; t++) {
    out[t] = GuardOut();
    out[t].acc = in[t].acc;
    out[t].overCount = in[t].overCount;
    over[t] = trigger[t] = false;
    if (!in[t].active || held(in[t], now))
      continue;
    // Current is ignored in the Inrush Window and on a PROG track: NMRA
    // S-9.2.3 limits it to 250mA sustained for more than 100ms, and an ack
    // pulse may be higher for 6ms, so the main loop looks after it.
    bool armed = (long)(now - in[t].armAt) >= 0;
    bool currentOver = in[t].currentOver && !in[t].isProg && armed;
    if (in[t].sampled)
      out[t].overCount = !currentOver ? 0
        : out[t].overCount < REVERSAL_SAMPLES ? out[t].overCount + 1 : REVERSAL_SAMPLES;
    over[t] = in[t].fault || currentOver;
    trigger[t] = in[t].fault || (currentOver && out[t].overCount >= REVERSAL_SAMPLES);
    if (over[t]) {
      out[t].acc += times.tickUs;
      anyOver = true;
      if (trigger[t]) {
        if (in[t].isAuto) autoTrigger = true;
        else if (!in[t].isProg) mainTrigger = true;
      }
    } else if (out[t].acc > times.tickUs) {
      out[t].acc -= times.tickUs;
    } else {
      out[t].acc = 0;
    }
  }
  if (!anyOver) return;

  // On a short reverse the AUTO track which has it. If no AUTO track has
  // a short and a plain track has one, the driver of the plain track may
  // be the one that sees a reverse loop short: reverse one loop, the one
  // with the highest last sample, a tie to the lowest index. A loop with
  // fewer than REVERSAL_SAMPLES over samples takes part like any other.
  // Should that be the wrong one, the short persists past the hold, the
  // first loop is locked out, and this picks the other.
  uint8_t pick = n;
  if (!autoTrigger && mainTrigger)
    for (uint8_t t = 0; t < n; t++)
      if (canReverse(in[t], now, times) && (pick == n || in[t].lastRaw > in[pick].lastRaw))
        pick = t;
  bool reversed = false;
  for (uint8_t t = 0; t < n; t++) {
    if ((trigger[t] || t == pick) && canReverse(in[t], now, times)) {
      out[t].reverse = true;
      reversed = true;
    }
  }
  if (reversed) {
    // every track gets the hold to recover
    for (uint8_t t = 0; t < n; t++) {
      out[t].hold = true;
      out[t].acc = 0;
      out[t].overCount = 0;
    }
    return;
  }
  // Reversing did not help or was not possible: an Overload
  for (uint8_t t = 0; t < n; t++)
    if (over[t] && out[t].acc >= times.fastTripUs)
      out[t].cut = true;
}
