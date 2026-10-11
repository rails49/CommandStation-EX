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
#include "HostTimeout.h"
#include "DCC.h"
#include "DIAG.h"
#include "TrackManager.h"

uint32_t HostTimeout::timeoutMs = 0;
uint32_t HostTimeout::lastSetMs = 0;

bool HostTimeout::set(int16_t seconds) {
  if (seconds < 0 || seconds > 3600) return false;
  timeoutMs = (uint32_t)seconds * 1000;
  lastSetMs = millis();
  return true;
}

void HostTimeout::loop() {
  if (timeoutMs == 0 || millis() - lastSetMs < timeoutMs) return;
  timeoutMs = 0;
  DCC::estopAll();
  TrackManager::setJoin(false);
  TrackManager::setTrackPower(TRACK_ALL, POWERMODE::OFF);
  DIAG(F("HOST TIMEOUT: TRACK POWER OFF"));
}
