//

#include "peaks/noise/brown.h"

#include <cstdio>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void BrownNoise::Init() {}

int16_t BrownNoise::ProcessSingleSample(uint8_t control1, uint8_t control2) {
  int16_t brown = 0;
	int16_t white = Random::GetSample();
	brown = (lastout_ + (white / 50)) * 0.95;
	lastout_ = brown;
	return brown; // (roughly) compensate for gain
}

}  // namespace peaks
