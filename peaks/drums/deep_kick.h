// Based on the Peaks BassDrum (Copyright 2013 Olivier Gillet)
//
// * Adds a LPF to the output.
// * Pot 3 now controls both punch and tone (pots 2 & 3 respectively on the stock drum)
// * Pot 2 controls the LPF cutoff
// * The stock kick drum replaces the snare drum in the UI
//

#ifndef PEAKS_DRUMS_DEEP_KICK_H_
#define PEAKS_DRUMS_DEEP_KICK_H_

#include "stmlib/stmlib.h"

#include "peaks/drums/svf.h"
#include "peaks/drums/excitation.h"

#include "peaks/gate_processor.h"

namespace peaks {

class DeepKick {
 public:
  DeepKick() { }
  ~DeepKick() { }

  void Init();
  int16_t ProcessSingleSample(uint8_t control) IN_RAM;

  void Configure(uint16_t* parameter, ControlMode control_mode) {
    if (control_mode == CONTROL_MODE_HALF) {
      set_frequency(0);
      set_punch(0);
      set_lpf_cutoff(40000);
      set_tone(8192 + (parameter[0] >> 1));
      set_decay(parameter[1]);
    } else {
      set_frequency(parameter[0] - 32768);
      set_lpf_cutoff(parameter[1]);
      set_punch(parameter[2]);
      set_tone(parameter[2]);
      set_decay(parameter[3]);
    }
  }

  void set_frequency(int16_t frequency) {
    frequency_ = (31 << 7) + (static_cast<int32_t>(frequency) * 896 >> 15);
  }

  void set_decay(uint16_t decay) {
    uint32_t scaled;
    uint32_t squared;
    scaled = 65535 - decay;
    squared = scaled * scaled >> 16;
    scaled = squared * scaled >> 18;
    resonator_.set_resonance(32768 - 128 - scaled);
  }

  void set_lpf_cutoff(uint16_t frequency) {
    int16_t base_note = 52 << 7;
    int32_t transposition = frequency;
    base_note += transposition * 896 >> 15;
    lpf_.set_frequency(base_note + (12 << 7));
  }

  void set_tone(uint16_t tone) {
    uint32_t coefficient = tone;
    coefficient = coefficient * coefficient >> 16;
    lp_coefficient_ = 512 + (coefficient >> 2) * 3;
  }

  void set_punch(uint16_t punch) {
    resonator_.set_punch(punch * punch >> 16);
  }

 private:
  Excitation pulse_up_;
  Excitation pulse_down_;
  Excitation attack_fm_;
  Svf resonator_;
  Svf lpf_;

  int32_t frequency_;
  int32_t lp_coefficient_;
  int32_t lp_state_;

  DISALLOW_COPY_AND_ASSIGN(DeepKick);
};

}  // namespace peaks

#endif  // PEAKS_DRUMS_DEEP_KICK_H_
