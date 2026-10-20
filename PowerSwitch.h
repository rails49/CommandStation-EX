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
#ifndef PowerSwitch_h
#define PowerSwitch_h

// When MotorDriver::setPower() drives the power pin and sets guardActive.
// No Arduino type may appear here: the native test in rails49/ compiles this
// file with the host compiler.

// Only when power goes on or off. wasOn and on are true for ON and ALERT.
// ON to ALERT and back leaves both alone, so a Guard cut stays cut until
// checkPowerOverload() reports it (#83).
inline bool powerSwitches(bool wasOn, bool on) {
  return on != wasOn;
}

#endif
