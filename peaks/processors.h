// Copyright 2013 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// This is the common entry points for all types of modulation sources!

#ifndef PEAKS_PROCESSORS_H_
#define PEAKS_PROCESSORS_H_

#include "stmlib/stmlib.h"

#include <algorithm>

#include "peaks/drums/bass_drum.h"
#include "peaks/noise/snare.h"
#include "peaks/noise/hh.h"
#include "peaks/noise/pink.h"
#include "peaks/noise/white.h"

#include "peaks/gate_processor.h"

namespace peaks {

enum ProcessorFunction {
  PROCESSOR_FUNCTION_WHITE_NOISE,
  PROCESSOR_FUNCTION_PINK_NOISE,
  PROCESSOR_FUNCTION_SNARE_NOISE,
  PROCESSOR_FUNCTION_HH_NOISE,
  PROCESSOR_FUNCTION_BASS_DRUM,
  PROCESSOR_FUNCTION_LAST
};

#define DECLARE_BUFFERED_PROCESSOR(ClassName, variable) \
  void ClassName ## Init() { \
    variable.Init(); \
  } \
  void ClassName ## FillBuffer() { \
    variable.FillBuffer(&input_buffer_, &output_buffer_); \
  } \
  void ClassName ## Configure(uint16_t* p, ControlMode control_mode) { \
    variable.Configure(p, control_mode); \
  } \
  ClassName variable;

#define DECLARE_UNBUFFERED_PROCESSOR(ClassName, variable) \
  void ClassName ## Init() { \
    variable.Init(); \
  } \
  int16_t ClassName ## ProcessSingleSample(uint8_t control1, uint8_t control2) { \
    return variable.ProcessSingleSample(control1, control2); \
  } \
  void ClassName ## Configure(uint16_t* p, ControlMode control_mode) { \
    variable.Configure(p, control_mode); \
  } \
  ClassName variable;


class Processors {
 public:
  Processors() { }
  ~Processors() { }

  void Init(uint8_t index);

  typedef void (Processors::*InitFn)();
  typedef int16_t (Processors::*ProcessSingleSampleFn)(uint8_t, uint8_t);
  typedef void (Processors::*FillBufferFn)();
  typedef void (Processors::*ConfigureFn)(uint16_t*, ControlMode);

  struct ProcessorCallbacks {
    InitFn init_fn;
    ProcessSingleSampleFn process_single_sample;
    FillBufferFn fill_buffer;
    ConfigureFn configure;
  };

  inline void set_control_mode(ControlMode control_mode) {
    control_mode_ = control_mode;
    Configure();
  }

  inline void set_parameter(uint8_t index, uint16_t parameter) {
    parameter_[index] = parameter;
    Configure();
  }

  inline void CopyParameters(uint16_t* parameters, uint16_t size) {
    std::copy(&parameters[0], &parameters[size], &parameter_[0]);
  }

  inline void set_function(ProcessorFunction function) {
    function_ = function;
    callbacks_ = callbacks_table_[function];
    Configure();
  }

  inline ProcessSingleSampleFn get_process_single_sample() {
    return callbacks_.process_single_sample;
  }

  inline int16_t ProcessBuffer(uint8_t control) {
    input_buffer_.Overwrite(control);
    return output_buffer_.ImmediateRead();
  }

  inline ProcessorFunction function() const { return function_; }

  inline int16_t Process(uint8_t control1, uint8_t control2) {
    return (this->*callbacks_.process_single_sample)(control1, control2);
  }

  inline bool Buffer() {
    if (callbacks_.fill_buffer) {
      if (output_buffer_.writable() < kBlockSize) {
        return false;
      } else {
        (this->*callbacks_.fill_buffer)();
        return true;
      }
    } else {
      return true;
    }
  }

 private:
  void Configure() {
    (this->*callbacks_.configure)(&parameter_[0], control_mode_);
  }

  InputBuffer input_buffer_;
  OutputBuffer output_buffer_;

  ControlMode control_mode_;
  ProcessorFunction function_;
  uint16_t parameter_[4];

  ProcessorCallbacks callbacks_;
  static const ProcessorCallbacks callbacks_table_[PROCESSOR_FUNCTION_LAST];

  DECLARE_UNBUFFERED_PROCESSOR(WhiteNoise, white_noise_);
  DECLARE_UNBUFFERED_PROCESSOR(PinkNoise, pink_noise_);
  DECLARE_UNBUFFERED_PROCESSOR(HHNoise, hh_noise_);
  DECLARE_UNBUFFERED_PROCESSOR(SnareNoise, snare_noise_);
  DECLARE_UNBUFFERED_PROCESSOR(BassDrum, bass_drum_);

  DISALLOW_COPY_AND_ASSIGN(Processors);
};

extern Processors processors[2];

}  // namespace peaks

#endif  // PEAKS_PROCESSORS_H_
