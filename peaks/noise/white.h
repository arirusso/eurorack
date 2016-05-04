//

#ifndef PEAKS_WHITE_NOISE_H_
#define PEAKS_WHITE_NOISE_H_

#include "stmlib/stmlib.h"

#include "peaks/gate_processor.h"

namespace peaks {

class WhiteNoise {
 public:
  WhiteNoise() { }
  ~WhiteNoise() { }

  void Init();
  int16_t ProcessSingleSample(uint8_t control) IN_RAM;
  void Configure(uint16_t* parameter, ControlMode control_mode) { }

 private:

  DISALLOW_COPY_AND_ASSIGN(WhiteNoise);
};

}  // namespace peaks

#endif  // PEAKS_WHITE_NOISE_H_
