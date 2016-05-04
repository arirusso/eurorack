//

#ifndef PEAKS_PINK_NOISE_H_
#define PEAKS_PINK_NOISE_H_

#include "stmlib/stmlib.h"

#include "peaks/gate_processor.h"

namespace peaks {

static const int32_t kFilterFactor[] = {
  100,
  100,
  103,
  115,
  181,
  -131
}

static const int32_t kWhiteFilterFactor[] = {
  1801,
  1331,
  649,
  322,
  187,
  -5917
}

static const uint16_t kFilterSumFactor = 186;
static const uint16_t kFilterIterateFactor = 862;
static const uint8_t kGainFactor = 9;

class PinkNoise {
 public:
  PinkNoise() { }
  ~PinkNoise() { }

  void Init();
  int16_t ProcessSingleSample(uint8_t control) IN_RAM;
  void Configure(uint16_t* parameter, ControlMode control_mode) { }

 private:
  static int16_t sum_array(int16_t a[], uint8_t num_elements) {
    uint8_t i, sum=0;
    for (i=0; i<num_elements; i++) {
	    sum = sum + a[i];
    }
    return(sum);
  }
  int16_t b_[7] = { 0, 0, 0, 0, 0, 0, 0 };

  DISALLOW_COPY_AND_ASSIGN(PinkNoise);
};

}  // namespace peaks

#endif  // PEAKS_PINK_NOISE_H_
