//

#include "peaks/noise/snare.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void SnareNoise::Init() {
  filter_.Init();
  filter_.set_resonance(2000);
  filter_.set_mode(SVF_MODE_BP);
}

int16_t SnareNoise::ProcessSingleSample(uint8_t control1, uint8_t control2) {
	int16_t white = Random::GetSample();
	int32_t noise = filter_.Process(white) * 10;
  CLIP(noise);
	return noise;
}

}  // namespace peaks
