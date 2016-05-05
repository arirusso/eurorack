// Based on the noise source from the Peaks high hat
// (Peaks high hat is copyright 2013 Olivier Gillet)

#include "peaks/noise/hh.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void HHNoise::Init() {
  noise_.Init();
  noise_.set_frequency(105 << 7);  // 8kHz
  noise_.set_resonance(24000);
  noise_.set_mode(SVF_MODE_BP);
}

int16_t HHNoise::ProcessSingleSample(uint8_t control) {
  phase_[0] += 48318382;
  phase_[1] += 71582788;
  phase_[2] += 37044092;
  phase_[3] += 54313440;
  phase_[4] += 66214079;
  phase_[5] += 93952409;

  int16_t noise = 0;
  noise += phase_[0] >> 31;
  noise += phase_[1] >> 31;
  noise += phase_[2] >> 31;
  noise += phase_[3] >> 31;
  noise += phase_[4] >> 31;
  noise += phase_[5] >> 31;
  noise <<= 12;

  // Run the SVF at the double of the original sample rate for stability.
  int32_t filtered_noise = 0;
  filtered_noise += noise_.Process(noise);
  filtered_noise += noise_.Process(noise);

  // The 808-style VCA amplifies only the positive section of the signal.
  if (filtered_noise < 0) {
    filtered_noise = 0;
  } else if (filtered_noise > 32767) {
    filtered_noise = 32767;
  }

  CLIP(filtered_noise);
  return filtered_noise;
}

}  // namespace peaks
