//

#ifndef PEAKS_SNARE_NOISE_H_
#define PEAKS_SNARE_NOISE_H_

#include "stmlib/stmlib.h"

#include "peaks/drums/svf.h"
#include "peaks/gate_processor.h"

namespace peaks {

class SnareNoise {
 public:
  SnareNoise() { }
  ~SnareNoise() { }

  void Init();
  int16_t ProcessSingleSample(uint8_t control1, uint8_t control2) IN_RAM;
  void Configure(uint16_t* parameter, ControlMode control_mode) { }

 private:
  Svf filter_;

  DISALLOW_COPY_AND_ASSIGN(SnareNoise);
};

}  // namespace peaks

#endif  // PEAKS_BROWN_NOISE_H_
