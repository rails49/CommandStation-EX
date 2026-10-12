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
#ifndef OverloadMean_h
#define OverloadMean_h
#include "CurrentMean.h"

// What MotorDriver::enterOverload() does to the mean <JI> reports. No
// Arduino type may appear here: the native test in rails49/ compiles this
// file with the host compiler.

// cut sets the track to OVERLOAD. The mean is cleared on every entry, so
// after a retry <JI> reports the samples since the retry, not the current
// that caused the Overload (#59).
template <class Cut>
inline void cutForOverload(CurrentMean &mean, Cut cut) {
  mean.reset();
  cut();
}

#endif
