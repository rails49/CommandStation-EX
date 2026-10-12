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
#ifndef TrackModeReport_h
#define TrackModeReport_h
#include <stdint.h>

// What a track reports as its Track Mode. No Arduino type may appear here:
// the native test in rails49/ compiles this file with the host compiler.
// See ADR-0005.

// The base modes whose reported INV is the Polarity the output has now,
// which a Reversal changes, TRACK_MODE_MAIN | TRACK_MODE_BOOST, and the INV
// bit, TRACK_MODIFIER_INV. TrackManager.cpp asserts they match MotorDriver.h.
// Where TRACK_MODE_BOOST is 0 no mode carries bit 32, so it is never matched.
static const uint8_t TRACK_MODE_REPORT_HARDWARE_POLARITY = 2 | 32;
static const uint8_t TRACK_MODE_REPORT_INV = 64;

// The mode as reported: on a MAIN or BOOST track INV is the hardware Polarity,
// not the one the host set. Every other mode is reported as it is.
inline uint8_t reportedModeWord(uint8_t mode, bool inverted) {
  if (!(mode & TRACK_MODE_REPORT_HARDWARE_POLARITY))
    return mode;
  return (mode & ~TRACK_MODE_REPORT_INV) | (inverted ? TRACK_MODE_REPORT_INV : 0);
}

#endif
