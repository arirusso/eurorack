//

#include "peaks/noise/pink.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void PinkNoise::Init() {
  uint8_t i;
  for (i = 0; i < 7; i += 1) {
    b_[i] = 0;
  }
}

int16_t PinkNoise::ProcessSingleSample(uint8_t control1, uint8_t control2) {
  int16_t white = Random::GetSample();
  uint8_t i;
  for (i = 0; i < 7; i += 1) {
    b_[i] = (b_[i] / kFilterFactor[0]) + (white / kWhiteFilterFactor[0]);
  }
  int16_t pink = PinkNoise::sum_array(b_, 7) + (white / kFilterSumFactor);
  pink /= kGainFactor; // (roughly) compensate for gain
  b_[6] = white / kFilterIterateFactor;
  CLIP(pink);
  return pink;
}

}  // namespace peaks
