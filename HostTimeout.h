/*
 *  © 2026 Bernhard Boser
 *  All rights reserved.
 *
 *  This file is part of DCC-EX
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
#ifndef HostTimeout_h
#define HostTimeout_h
#include <Arduino.h>

// <G n> sets a host timeout of n seconds and restarts it; <G 0> turns it off.
// Only <G n> restarts it. On expiry every loco is stopped as by <!>, every
// track is powered off as by <0>, and the timeout turns off.
class HostTimeout {
  public:
    static bool set(int16_t seconds);  // false if seconds is out of range
    static void loop();
  private:
    static uint32_t timeoutMs;  // 0 when off
    static uint32_t lastSetMs;
};

#endif
