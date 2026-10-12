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
#ifndef TrackModeChange_h
#define TrackModeChange_h
#include <stdint.h>

// Which mode commands TrackManager::setTrackMode() handles in place. No
// Arduino type may appear here: the native test in rails49/ compiles this
// file with the host compiler. See ADR-0005.

// The bits of a Track Mode word that change only the Polarity and whether
// the Guard may reverse the track, TRACK_MODIFIER_INV | TRACK_MODIFIER_AUTO,
// and the bit of a DC track, TRACK_MODE_DC. TrackManager.cpp asserts they
// match MotorDriver.h.
static const uint8_t TRACK_MODE_CHANGE_MODIFIERS = 64 | 128;
static const uint8_t TRACK_MODE_CHANGE_DC = 8;

// A change of INV or AUTO alone, or a repeat of the same mode, and on a DC
// track with the DC address unchanged. Such a change keeps the power on and the
// signal pins attached; setTrackMode() only sets the mode and the Polarity.
inline bool isModifierOnlyChange(uint8_t oldMode, uint8_t mode,
                                 int16_t oldDcAddr, int16_t dcAddr) {
  if ((mode & ~TRACK_MODE_CHANGE_MODIFIERS) !=
      (oldMode & ~TRACK_MODE_CHANGE_MODIFIERS))
    return false;
  return !(mode & TRACK_MODE_CHANGE_DC) || oldDcAddr == dcAddr;
}

#endif
