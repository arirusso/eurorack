//

#ifndef PEAKS_HIGH_HAT_NOISE_H_
#define PEAKS_HIGH_HAT_NOISE_H_

#include "stmlib/stmlib.h"

namespace peaks {

class HighHatNoise {
 public:
  HighHatNoise() { }
  ~HighHatNoise() { }

  void Init();
  int16_t ProcessSingleSample(uint8_t control) IN_RAM;
  void Configure(uint16_t* parameter, ControlMode control_mode) { }

 private:

  DISALLOW_COPY_AND_ASSIGN(HighHatNoise);
};

}  // namespace peaks

#endif  // PEAKS_HIGH_HAT_NOISE_H_
