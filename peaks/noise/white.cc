//

#include "peaks/noise/white.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void WhiteNoise::Init() { }

int16_t WhiteNoise::ProcessSingleSample(uint8_t control) {

  int16_t noise = Random::GetSample();
  noise /= 2; // gain compensation
  CLIP(noise);
  return noise;
}

}  // namespace peaks
