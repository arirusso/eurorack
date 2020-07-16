// Copyright 2013 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Mini sequencer.

#ifndef PEAKS_LOW_FREQUENCY_COUNTER_H_
#define PEAKS_LOW_FREQUENCY_COUNTER_H_

#include "stmlib/stmlib.h"

#include <algorithm>

#include "peaks/gate_processor.h"

namespace peaks {

class LowFrequencyCounter {
 public:
  LowFrequencyCounter() { }
  ~LowFrequencyCounter() { }

  void Init() {
    counter_ = 0;
  }

  inline void set_range(uint8_t value) {
    range_ = value;
  }

  void Configure(uint16_t* parameter, ControlMode control_mode) {
    set_range(parameter[0] - 32768);
  }

  inline int16_t ProcessSingleSample(uint8_t control) {
    counter_++;
    if (control & CONTROL_GATE_RISING) {
      output_ += 100;
      if (output_ >= 32768) {
        output_ = 32768;
      }
    } else {
      output_ -= 100; // * range
      if (output_ <= 0) {
        output_ = 0;
      }
    }
    return output_;
  }

 private:
  uint8_t range_;
  int16_t counter_;
  int16_t output_;

  DISALLOW_COPY_AND_ASSIGN(LowFrequencyCounter);
};

}  // namespace peaks

#endif  // PEAKS_LOW_FREQUENCY_COUNTER_H_
