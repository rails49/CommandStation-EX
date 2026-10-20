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
#ifndef PowerLine_h
#define PowerLine_h

// What TrackManager::getPower(t, s) reports for a track in the power lines.
// No Arduino type may appear here: the native test in rails49/ compiles this
// file with the host compiler.

// '1' only for a track that is ON. A track in ALERT has power but reads '0',
// so p1Due is set: checkPowerOverload() Broadcasts the power lines when the
// track is back ON (#78).
inline char readPowerLine(bool on, bool alert, bool &p1Due) {
  if (alert)
    p1Due = true;
  return on ? '1' : '0';
}

#endif
