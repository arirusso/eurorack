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
// 808-style bass drum.

#include "peaks/drums/deep_kick.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void DeepKick::Init() {
  pulse_up_.Init();
  pulse_down_.Init();
  attack_fm_.Init();
  resonator_.Init();
  lpf_.Init();

  pulse_up_.set_delay(0);
  pulse_up_.set_decay(3340);

  pulse_down_.set_delay(1.0e-3 * 48000);
  pulse_down_.set_decay(3072);

  attack_fm_.set_delay(4.0e-3 * 48000);
  attack_fm_.set_decay(4093);

  resonator_.set_punch(32768);
  resonator_.set_mode(SVF_MODE_BP);

  lpf_.set_punch(0);
  lpf_.set_mode(SVF_MODE_LP);
  lpf_.set_resonance(1);

  set_frequency(0);
  set_decay(32768);
  set_tone(32768);
  set_punch(65535);
  set_lpf_cutoff(40000);

  lp_state_ = 0;
}

int16_t DeepKick::ProcessSingleSample(uint8_t control) {
  if (control & CONTROL_GATE_RISING) {
    pulse_up_.Trigger(12 * 32768 * 0.7);
    pulse_down_.Trigger(-19662 * 0.7);
    attack_fm_.Trigger(18000);
  }
  int32_t excitation = 0;
  excitation += pulse_up_.Process();
  excitation += !pulse_down_.done() ? 16384 : 0;
  excitation += pulse_down_.Process();
  attack_fm_.Process();
  resonator_.set_frequency(frequency_ + (attack_fm_.done() ? 0 : 17 << 7));

  int32_t resonator_output = (excitation >> 4) + resonator_.Process(excitation);
  lp_state_ += (resonator_output - lp_state_) * lp_coefficient_ >> 15;
  int32_t output = lpf_.Process(lp_state_);
  CLIP(output);
  return output;
}

}  // namespace peaks
