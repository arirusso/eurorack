// Based on the noise source from the Peaks high hat
// (Peaks high hat is copyright 2013 Olivier Gillet)

#ifndef PEAKS_HH_NOISE_H_
#define PEAKS_HH_NOISE_H_

#include "stmlib/stmlib.h"
#include "peaks/drums/svf.h"

#include "peaks/gate_processor.h"

namespace peaks {

class HHNoise {
 public:
  HHNoise() { }
  ~HHNoise() { }

  void Init();
  int16_t ProcessSingleSample(uint8_t control1, uint8_t control2) IN_RAM;
  void Configure(uint16_t* parameter, ControlMode control_mode) { }

 private:
  Svf noise_;
  uint32_t phase_[6];

  DISALLOW_COPY_AND_ASSIGN(HHNoise);
};

}  // namespace peaks

#endif  // PEAKS_HH_NOISE_H_
