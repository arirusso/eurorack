//

#include "peaks/pulse_processor/CLOCK_DIVIDER.h"

#include <algorithm>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

#include "peaks/resources.h"

namespace peaks {

using namespace stmlib;

void ClockDivider::Init() {
  counter_ = 0;
  last_pulse_at_ = NULL;
}

void ClockDivider::FillBuffer(
    InputBuffer* input_buffer,
    OutputBuffer* output_buffer) {
  bool is_new_pulse = false;
  bool should_output = false;

  for (uint8_t i = 0; i < kBlockSize; ++i) {
    uint8_t control = input_buffer->ImmediateRead();
    is_new_pulse |= control & CONTROL_GATE_RISING;
  }

  if (is_new_pulse) {
    if (last_pulse_at_ != NULL) {
      period_ = counter_ - last_pulse_at_;
    }
    last_pulse_at_ = counter;
  }
// this is actually a clock multiplier
  should_output = ((counter_ - period_) % 2 == 0);
// todo: counter will overflow
  counter_ += 1;

  // do output
  uint16_t output = should_output ? 20480 : 0;

  for (uint8_t i = 0; i < kBlockSize; ++i) {
    output_buffer->Overwrite(output);
  }
}

}  // namespace peaks
