//
// VC Clock div/mult
// Alternate firmware for Mutable Instruments Branches
//
// Based on code from the original Mutable Instruments Branches firmware
// Branches is Copyright 2012 Olivier Gillet.
//

#include <avr/eeprom.h>
#include <avr/pgmspace.h>

#include "avrlib/adc.h"
#include "avrlib/boot.h"
#include "avrlib/gpio.h"
#include "avrlib/watchdog_timer.h"

using namespace avrlib;

enum LedState {
  LED_STATE_OFF,
  LED_STATE_RED,
  LED_STATE_GREEN
};

enum AdcChannel {
  ADC_CHANNEL_2_CV,
  ADC_CHANNEL_1_CV,
  ADC_CHANNEL_LAST
};

Gpio<PortD, 4> in_1;
Gpio<PortD, 3> out_1_a;
Gpio<PortD, 0> out_1_b;
Gpio<PortD, 1> led_1_a;
Gpio<PortD, 2> led_1_k;

Gpio<PortD, 7> in_2;
Gpio<PortD, 6> out_2_a;
Gpio<PortD, 5> out_2_b;
Gpio<PortB, 1> led_2_a;
Gpio<PortB, 0> led_2_k;

Gpio<PortC, 2> switch_2;
Gpio<PortC, 3> switch_1;

AdcInputScanner adc;

const uint16_t kLongPressTime = 6250;  // 800 * 8000 / 1024
const uint16_t kLedClockGateDuration = 0x100;
const uint16_t kLedFactoredGateDuration = 0x100;

static uint8_t adc_channel;

bool gate_input_state[2];
bool switch_state[2];
bool inhibit_switch[2];

uint16_t press_time[2];
uint8_t led_state[2];
uint16_t led_gate_duration[2];

const uint8_t kPulseTrackerBufferSize = 10;
const uint8_t kMultiplyErrorCorrectionAmount = 12;
const uint8_t kAdcRatio = 5; // 1:5
const int8_t kFactors[] = { -8, -7, -6, -5, -4, -3, -2, 0, 2, 3, 4, 5, 6, 7, 8 };

uint16_t pulse_tracker_buffer[2][kPulseTrackerBufferSize];
uint16_t pulse_tracker_period_[2];

int8_t divide_counter[2];
bool multiply_is_debouncing[2];

uint16_t last_output_time[2];
uint8_t adc_counter;

int8_t factor[2];
int16_t factor_control_value[2];
uint8_t factors_index_ratio;
uint8_t factors_num_available;

void GateInputsInit() {
  in_1.set_mode(DIGITAL_INPUT);
  in_2.set_mode(DIGITAL_INPUT);
  in_1.High();
  in_2.High();
}

void SwitchesInit() {
  switch_1.set_mode(DIGITAL_INPUT);
  switch_2.set_mode(DIGITAL_INPUT);
  switch_1.High();
  switch_2.High();

  switch_state[0] = switch_state[1] = false;
}

void GateOutputsInit() {
  out_1_a.set_mode(DIGITAL_OUTPUT);
  out_1_b.set_mode(DIGITAL_OUTPUT);
  out_2_a.set_mode(DIGITAL_OUTPUT);
  out_2_b.set_mode(DIGITAL_OUTPUT);
}

void LedsInit() {
  led_1_a.set_mode(DIGITAL_OUTPUT);
  led_1_k.set_mode(DIGITAL_OUTPUT);

  led_2_a.set_mode(DIGITAL_OUTPUT);
  led_2_k.set_mode(DIGITAL_OUTPUT);

  led_1_a.Low();
  led_2_a.Low();
  led_1_k.Low();
  led_2_k.Low();

  led_state[0] = led_state[1] = 0;
}

void AdcInit() {
  adc.Init();
  adc.set_num_inputs(ADC_CHANNEL_LAST);
  Adc::set_reference(ADC_DEFAULT);
  Adc::set_alignment(ADC_LEFT_ALIGNED);
  adc_counter = 1;
}

void FactorInit() {
  factors_num_available = sizeof(kFactors)/sizeof(kFactors[0]);
  factors_index_ratio = 255/(factors_num_available-1);
}

void Init() {
  Gpio<PortB, 4>::set_mode(DIGITAL_OUTPUT);
  Gpio<PortB, 4>::Low();

  GateInputsInit();
  SwitchesInit();
  GateOutputsInit();
  LedsInit();
  AdcInit();

  TCCR1A = 0;
  TCCR1B = 5;

  FactorInit();
}

bool Read(uint8_t channel) {
  return channel == 0 ? !in_1.value() : !in_2.value();
}

bool ReadSwitch(uint8_t channel) {
  return channel == 0 ? !switch_1.value() : !switch_2.value();
}

void GateOn(uint8_t channel) {
  switch (channel) {
    case 0: out_1_a.High();
            out_1_b.High();
            break;
    case 1: out_2_a.High();
            out_2_b.High();
            break;
  }
}

void GateOff(uint8_t channel) {
  switch (channel) {
    case 0: out_1_a.Low();
            out_1_b.Low();
            break;
    case 1: out_2_a.Low();
            out_2_b.Low();
            break;
  }
}

void PulseTrackerClear(uint8_t channel) {
  for (uint8_t i = 0; i < kPulseTrackerBufferSize; ++i) {
    pulse_tracker_buffer[channel][i] = 0;
  }
}

uint16_t PulseTrackerGetElapsed(uint8_t channel) {
  return TCNT1 - pulse_tracker_buffer[channel][kPulseTrackerBufferSize - 1];
}

// The period of time between the last two recorded events
uint16_t PulseTrackerGetPeriod(uint8_t channel) {
  if (pulse_tracker_period_[channel] == 0) {
    pulse_tracker_period_[channel] = pulse_tracker_buffer[channel][kPulseTrackerBufferSize - 1] - pulse_tracker_buffer[channel][kPulseTrackerBufferSize - 2];
  }
  return pulse_tracker_period_[channel];
}

void PulseTrackerRecord(uint8_t channel) {
  if (pulse_tracker_buffer[channel][kPulseTrackerBufferSize - 1] != TCNT1) {
    pulse_tracker_period_[channel] = 0;
    // shift
    for (uint8_t i = kPulseTrackerBufferSize-1; i > 0; i--) {
      if (pulse_tracker_buffer[channel][i] > 0) {
        pulse_tracker_buffer[channel][i-1] = pulse_tracker_buffer[channel][i];
      }
    }
    // record
    pulse_tracker_buffer[channel][kPulseTrackerBufferSize - 1] = TCNT1;
  }
}

// Is the factor control setting in bypass mode? (zero)
bool BypassIsEnabled(uint8_t channel) {
  return factor[channel] == 0;
}

// Is the factor control setting such that we're in multiplier mode?
bool MultiplyIsEnabled(uint8_t channel) {
  return factor[channel] < 0;
}

// Have enough events been recorded that it's possible to multiply ?
bool MultiplyIsPossible(uint8_t channel) {
  return (pulse_tracker_buffer[channel][kPulseTrackerBufferSize - 1] > 0 && pulse_tracker_buffer[channel][kPulseTrackerBufferSize - 2] > 0);
}

// The time interval between multiplied events
// eg if clock is comes in at 100 and 200, and the clock multiply factor is 2,
// the result will be 50
uint16_t MultiplyInterval(uint8_t channel) {
  return (PulseTrackerGetPeriod(channel) / -factor[channel]);
}

bool MultiplyShouldDoOutput(uint8_t channel, uint16_t elapsed) {
  uint16_t interval = MultiplyInterval(channel);
  multiply_is_debouncing[channel] = (!multiply_is_debouncing[channel] &&
    (elapsed % interval <= kMultiplyErrorCorrectionAmount));
  return multiply_is_debouncing[channel];
}

// Is the factor control setting such that we're in divider mode?
bool DivideIsEnabled(uint8_t channel) {
  return factor[channel] > 0;
}

bool DivideShouldDoOutput(uint8_t channel) {
  return (divide_counter[channel] == (factor[channel] - 1));
}

void FactorUpdate(uint8_t channel) {
  uint16_t factor_index = factors_num_available - (factor_control_value[channel]/factors_index_ratio);
  // correcting for weird adc value
  factor_index = (factor_index == 0) ? factors_num_available - 1 : factor_index - 1;
  //
  factor[channel] = kFactors[factor_index];
}

void AdcScan() {
  if (adc_counter == (kAdcRatio-1)) {
    adc.Scan();
    adc_counter = 0;
  } else {
    ++adc_counter;
  }
}

void AdcPoll(uint8_t channel) {
  if (adc_counter == (kAdcRatio-1)) {
    int16_t value = adc.Read8((channel == 0) ? 1 : 0);
    int16_t delta = value - factor_control_value[channel];
    // abs
    if (delta < 0) {
      delta = -delta;
    }
    if (delta > 12) {
      factor_control_value[channel] = value;
      FactorUpdate(channel);
    }
  }
}

void ClockHandleOverflow(uint8_t channel) {
  if (last_output_time[channel] > TCNT1) {
    PulseTrackerClear(channel);
    pulse_tracker_period_[channel] = 0;
    last_output_time[channel] = 0;
  }
}

void LedOff(uint8_t channel) {
  switch(channel) {
    case 0: led_1_a.Low();
            led_1_k.Low();
            break;
    case 1: led_2_a.Low();
            led_2_k.Low();
            break;
  }
}

void LedGreen(uint8_t channel) {
  switch(channel) {
    case 0: led_1_a.Low();
            led_1_k.High();
            break;
    case 1: led_2_a.Low();
            led_2_k.High();
            break;
  }
}

void LedRed(uint8_t channel) {
  switch(channel) {
    case 0: led_1_a.High();
            led_1_k.Low();
            break;
    case 1: led_2_a.High();
            led_2_k.Low();
            break;
  }
}

int main(void) {

  ResetWatchdog();
  Init();

  gate_input_state[0] = gate_input_state[1] = false;
  while (1) {
    AdcScan();

    // Scan inputs
    for (uint8_t i = 0; i < 2; ++i) {
      bool new_gate_input_state = Read(i);
      bool should_output_clock = false;
      bool should_output = false;

      AdcPoll(i);
      ClockHandleOverflow(i);

      if (new_gate_input_state && !gate_input_state[i] /* Rising edge */) {
        PulseTrackerRecord(i);
        // clock divider
        if (DivideIsEnabled(i)) {
          if (DivideShouldDoOutput(i)) {
            should_output = true;
            last_output_time[i] = TCNT1;
            divide_counter[i] = 0;
          } else {
            ++divide_counter[i];
          }
        } else {
          divide_counter[i] = 0;
          multiply_is_debouncing[i] = false;
          last_output_time[i] = TCNT1;
          should_output = true;
          should_output_clock = true;
        }
      }
      if (MultiplyIsEnabled(i) &&
            MultiplyIsPossible(i) &&
            MultiplyShouldDoOutput(i, PulseTrackerGetElapsed(i))) {
        last_output_time[i] = TCNT1;
        should_output = true;
      }

      // do stuff
      if (should_output) {
        GateOn(i);
        if (should_output_clock) {
          led_gate_duration[i] = kLedClockGateDuration;
          led_state[i] = 1;
        } else {
          led_gate_duration[i] = kLedFactoredGateDuration;
          led_state[i] = 2;
        }
      } else {
        GateOff(i);
      }
      gate_input_state[i] = new_gate_input_state;

      if (led_gate_duration[i]) {
        --led_gate_duration[i];
        if (!led_gate_duration[i]) {
          led_state[i] = 0;
        }
      }

      // Update Leds
      switch (led_state[i]) {
        case 0: LedOff(i);
                break;
        case 1: LedGreen(i);
                break;
        case 2: LedRed(i);
                break;
      }
    }

  }
}
