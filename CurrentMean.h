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
#ifndef CurrentMean_h
#define CurrentMean_h

#include <stdint.h>

// The mean of a track's current that <JI> reports: of the samples in the last
// complete window of WINDOW_US, 250 ms. A read does not reset it, so every
// client polling <JI> reads the same value at the same moment. Before the
// first window closes it is the mean of the samples so far. No Arduino type
// may appear here: the native test in rails49/ compiles this file with the
// host compiler.
class CurrentMean {
public:
  static const unsigned long WINDOW_US = 250000UL;

  // A sample in raw ADC counts, taken at now in microseconds.
  void add(unsigned int raw, unsigned long now) {
    if (count == 0 && !haveLast)
      start = now;
    else if (now - start >= WINDOW_US) {
      if (count) {
        last = sum / count;
        haveLast = true;
      }
      sum = 0;
      count = 0;
      start = now;
    }
    sum += raw;
    if (++count == SAMPLES_MAX) {
      // a long window: keep a mean weighted to recent samples
      sum >>= 1;
      count >>= 1;
    }
  }

  // False while there is no sample since the last reset.
  bool mean(int &out) const {
    if (haveLast)
      out = last;
    else if (count)
      out = sum / count;
    else
      return false;
    return true;
  }

  void reset() {
    sum = 0;
    count = 0;
    haveLast = false;
  }

private:
  // 4095 * 2^20 fits in 32 bits.
  static const uint32_t SAMPLES_MAX = 1UL << 20;
  uint32_t sum = 0;
  uint32_t count = 0;
  unsigned long start = 0;
  int last = 0;
  bool haveLast = false;
};

#endif
