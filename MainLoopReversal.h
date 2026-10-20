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
#ifndef MainLoopReversal_h
#define MainLoopReversal_h

// The order MotorDriver::checkPowerOverload() follows when the main loop sees
// a short on a track that is ON. No Arduino type may appear here: the native
// test in rails49/ compiles this file with the host compiler.

// The main loop reverses an AUTO track only if the Guard does not watch it,
// else the Guard does the Reversal. guardWatches is guardEligible(): a Guard
// cut clears guardActive on a track the Guard still watches (#83).
inline bool mainLoopReverses(bool autoTrack, bool guardWatches) {
  return autoTrack && !guardWatches;
}

// reverse flips the Polarity and Broadcasts the track state, alert sets the
// track to ALERT. A Reversal is Broadcast while the track is still ON, so the
// power lines say <p1 x>: ALERT returning to ON Broadcasts nothing, unless
// power lines went out during the ALERT (#78).
template <class Reverse, class Alert>
inline void alertFromOn(bool reversal, Reverse reverse, Alert alert) {
  if (reversal)
    reverse();
  alert();
}

#endif
