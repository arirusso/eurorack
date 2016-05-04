
//

#ifndef PEAKS_PULSE_PROCESSOR_CLOCK_DIVIDER_H_
#define PEAKS_PULSE_PROCESSOR_CLOCK_DIVIDER_H_

#include "stmlib/stmlib.h"
#include "stmlib/utils/ring_buffer.h"

#include "peaks/gate_processor.h"

namespace peaks {

struct TriggerPulse {
  uint32_t delay_counter;
};

static const uint8_t kTriggerPulseBufferSize = 32;

class ClockDivider {
 public:
  ClockDivider() { }
  ~ClockDivider() { }

  void Init();

  void Configure(uint16_t* parameter, ControlMode control_mode) {
    if (control_mode == CONTROL_MODE_HALF) {

    } else {

    }
  }

 private:

  uint16_t counter_;
  uint16_t last_pulse_at_;
  uint16_t period_;

  DISALLOW_COPY_AND_ASSIGN(ClockDivider);
};

}  // namespace peaks

#endif  // PEAKS_PULSE_PROCESSOR_CLOCK_DIVIDER_H_
