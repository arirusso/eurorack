//

#include "peaks/noise/pink.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void PinkNoise::Init() {
  for (uint8_t i = 0; i <= 6; i++) {
    b_[i] = 0;
  }
}

int16_t PinkNoise::ProcessSingleSample(uint8_t control1, uint8_t control2) {
  int16_t white = Random::GetSample();
  for (uint8_t i = 0; i < 6; i ++) {
    b_[i] = b_[i] / kFilterFactor[i] + (white / kWhiteFilterFactor[i]);
  }
  int16_t pink = PinkNoise::sum_array(b_, 7) + (white / kFilterSumFactor);
  pink *= kGainFactor; // (roughly) compensate for gain
  b_[6] = white / kFilterIterateFactor;
  CLIP(pink);
  return pink;
}

}  // namespace peaks
