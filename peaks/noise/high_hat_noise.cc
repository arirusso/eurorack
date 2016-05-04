//

#include "peaks/noise/high_hat.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void HighHatNoise::Init() { }

int16_t HighHatNoise::ProcessSingleSample(uint8_t control) {

  int16_t noise = Random::GetSample();

  CLIP(noise);
  return noise;
}

}  // namespace peaks
