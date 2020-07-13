//

#ifndef PEAKS_BROWN_NOISE_H_
#define PEAKS_BROWN_NOISE_H_

#include "stmlib/stmlib.h"

#include "peaks/gate_processor.h"

namespace peaks {

class BrownNoise {
 public:
  BrownNoise() { }
  ~BrownNoise() { }

  void Init();
  int16_t ProcessSingleSample(uint8_t control1, uint8_t control2) IN_RAM;
  void Configure(uint16_t* parameter, ControlMode control_mode) { }

 private:
  int16_t lastout_;

  DISALLOW_COPY_AND_ASSIGN(BrownNoise);
};

}  // namespace peaks

#endif  // PEAKS_BROWN_NOISE_H_
