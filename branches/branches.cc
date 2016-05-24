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
  LED_STATE_GREEN,
  LED_STATE_RED
};

enum ChannelMode {
  CHANNEL_FUNCTION_FACTORER,
  CHANNEL_FUNCTION_SWING,
  CHANNEL_FUNCTION_LAST
};

const uint8_t kSystemNumChannels = 2;

const ChannelMode function_table_[kSystemNumChannels] = {
  CHANNEL_FUNCTION_SWING,
  CHANNEL_FUNCTION_FACTORER
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
const uint16_t kLedThruGateDuration = 0x100;
const uint16_t kLedFactoredGateDuration = 0x100;

static uint8_t adc_channel;

bool gate_input_state[kSystemNumChannels];
bool switch_state[kSystemNumChannels];
bool inhibit_switch[kSystemNumChannels];

uint16_t press_time[kSystemNumChannels];
uint8_t led_state[kSystemNumChannels];
uint16_t led_gate_duration[kSystemNumChannels];

const uint8_t kPulseTrackerBufferSize = 6;
const uint8_t kTimingErrorCorrectionAmount = 12;
const uint8_t kAdcPollRatio = 5; // 1:5
const uint8_t kAdcDeltaThreshold = 12;
const int8_t kFactors[] = { -8, -7, -6, -5, -4, -3, -2, 0, 2, 3, 4, 5, 6, 7, 8 };
const int8_t kSwingFactorMin = 50;
const int8_t kSwingFactorMax = 70;

uint16_t pulse_tracker_buffer[kSystemNumChannels][kPulseTrackerBufferSize];
uint16_t pulse_tracker_period_[kSystemNumChannels];

bool multiply_is_debouncing[kSystemNumChannels];
bool swing_is_debouncing[kSystemNumChannels];
int8_t divide_counter[kSystemNumChannels];
int8_t swing_counter[kSystemNumChannels];

uint16_t last_output_at[kSystemNumChannels];
uint16_t last_thru_at[kSystemNumChannels];
uint8_t adc_counter;

int8_t factor[kSystemNumChannels];
int16_t factor_control_value[kSystemNumChannels];
uint8_t factors_index_ratio;
uint8_t factors_num_available;

int16_t swing[kSystemNumChannels];

void GateInputsInit() {
  in_1.set_mode(DIGITAL_INPUT);
  in_2.set_mode(DIGITAL_INPUT);
  in_1.High();
  in_2.High();

  gate_input_state[0] = gate_input_state[1] = false;
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

inline bool GateInputRead(uint8_t channel) {
  return channel == 0 ? !in_1.value() : !in_2.value();
}

bool SwitchRead(uint8_t channel) {
  return channel == 0 ? !switch_1.value() : !switch_2.value();
}

inline void GateOutputOn(uint8_t channel) {
  switch (channel) {
    case 0: out_1_a.High();
            out_1_b.High();
            break;
    case 1: out_2_a.High();
            out_2_b.High();
            break;
  }
}

inline void GateOutputOff(uint8_t channel) {
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
  //pulse_tracker_period_[channel] = 0;
}

inline uint16_t PulseTrackerGetElapsed(uint8_t channel) {
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

// Is the pulse tracker populated with enough events to perform multiply?
bool MultiplyIsPossible(uint8_t channel) {
  return pulse_tracker_buffer[channel][kPulseTrackerBufferSize - 1] > 0 &&
    pulse_tracker_buffer[channel][kPulseTrackerBufferSize - 2] > 0;
}

// The time interval between multiplied events
// eg if clock is comes in at 100 and 200, and the clock multiply factor is 2,
// the result will be 50
uint16_t MultiplyInterval(uint8_t channel) {
  return PulseTrackerGetPeriod(channel) / -factor[channel];
}

bool MultiplyShouldExec(uint8_t channel, uint16_t elapsed) {
  uint16_t interval = MultiplyInterval(channel);
  return (!multiply_is_debouncing[channel] && (elapsed % interval <= kTimingErrorCorrectionAmount));
}

// Is the factor control setting such that we're in divider mode?
bool DivideIsEnabled(uint8_t channel) {
  return factor[channel] > 0;
}

bool DivideShouldExec(uint8_t channel) {
  return divide_counter[channel] >= (factor[channel] - 1);
}

void DivideReset(uint8_t channel) {
  divide_counter[channel] = 0;
}

int8_t FactorGet(uint8_t channel) {
  uint16_t factor_index = factors_num_available - (factor_control_value[channel]/factors_index_ratio);
  // correcting for weird adc value
  factor_index = (factor_index == 0) ? factors_num_available - 1 : factor_index - 1;
  //
  return kFactors[factor_index];
}

inline void LedOff(uint8_t channel) {
  switch(channel) {
    case 0: led_1_a.Low();
            led_1_k.Low();
            break;
    case 1: led_2_a.Low();
            led_2_k.Low();
            break;
  }
}

inline void LedGreen(uint8_t channel) {
  switch(channel) {
    case 0: led_1_a.Low();
            led_1_k.High();
            break;
    case 1: led_2_a.Low();
            led_2_k.High();
            break;
  }
}

inline void LedRed(uint8_t channel) {
  switch(channel) {
    case 0: led_1_a.High();
            led_1_k.Low();
            break;
    case 1: led_2_a.High();
            led_2_k.Low();
            break;
  }
}

void AdcScan() {
  if (adc_counter == (kAdcPollRatio-1)) {
    adc.Scan();
    adc_counter = 0;
  } else {
    ++adc_counter;
  }
}

inline int16_t AdcReadValue(uint8_t channel) {
  uint8_t pin = (channel == 0) ? 1 : 0;
  return adc.Read8(pin);
}

bool AdcHasNewValue(uint8_t channel) {
  if (adc_counter == 0) {
    int16_t value = AdcReadValue(channel);
    // compare to stored factor control value
    int16_t delta = value - factor_control_value[channel];
    // abs
    if (delta < 0) {
      delta = -delta;
    }
    if (delta > kAdcDeltaThreshold) {
      // store factor control value
      factor_control_value[channel] = value;
      return true;
    }
  }
  return false;
}

inline bool ClockIsOverflow() {
  for (uint8_t i = 0; i < kSystemNumChannels; ++i) {
    if (last_output_at[i] > TCNT1) {
      return true;
    }
  }
  return false;
}

void ClockHandleOverflow() {
  for (uint8_t i = 0; i < kSystemNumChannels; ++i) {
    PulseTrackerClear(i);
    last_output_at[i] = 0;
  }
}

inline void LedExecThru(uint8_t channel) {
  led_gate_duration[channel] = kLedThruGateDuration;
  led_state[channel] = LED_STATE_GREEN;
}

inline void LedExecFactored(uint8_t channel) {
  led_gate_duration[channel] = kLedFactoredGateDuration;
  led_state[channel] = LED_STATE_RED;
}

inline void LedsUpdate(uint8_t channel) {
  //
  if (led_gate_duration[channel]) {
    --led_gate_duration[channel];
    if (!led_gate_duration[channel]) {
      led_state[channel] = 0;
    }
  }

  if (factor_control_value[channel] != 0) {

}
  /*} else if (factor[channel] == 0) {
    LedOff(channel);
  } else {
    LedGreen(channel);
  }*/

/*
  // Update Leds
  switch (led_state[channel]) {
    case LED_STATE_OFF: LedOff(channel);
                        break;
    case LED_STATE_GREEN: LedGreen(channel);
                          break;
    case LED_STATE_RED: LedRed(channel);
                        break;
  }
  */
}

inline bool GateInputIsRisingEdge(uint8_t channel) {
  bool last_state = gate_input_state[channel];
  // store current input state
  gate_input_state[channel] = GateInputRead(channel);
  //
  return gate_input_state[channel] && !last_state;
}

inline void MultiplyExec(uint8_t channel) {
  if (MultiplyIsEnabled(channel) &&
        MultiplyIsPossible(channel) &&
        MultiplyShouldExec(channel, PulseTrackerGetElapsed(channel))) {
    last_output_at[channel] = TCNT1;
    multiply_is_debouncing[channel] = true;
  }
}

inline void FactorerHandleInputGateRisingEdge(uint8_t channel) {
  if (DivideIsEnabled(channel)) {
    if (DivideShouldExec(channel)) {
      DivideReset(channel);
      last_output_at[channel] = TCNT1;
    } else {
      ++divide_counter[channel];
    }
  } else { // thru
    DivideReset(channel);
    last_output_at[channel] = TCNT1;
    last_thru_at[channel] = TCNT1;
    multiply_is_debouncing[channel] = false;
  }
}

int8_t SwingGet(uint8_t channel) {
  return ((factor_control_value[channel] * (kSwingFactorMax - kSwingFactorMin)) / 255) + kSwingFactorMin;
}

inline uint16_t SwingInterval(uint8_t channel) {
  uint16_t period = PulseTrackerGetPeriod(channel);
  uint16_t ratio = 1000 / swing[channel];
  uint16_t scaled = (10 * (period * 2)) / ratio;
  return scaled - period;
}

inline bool SwingShouldExec(uint8_t channel, uint16_t elapsed) {
  if (swing_counter[channel] == 2 && swing[channel] != 50) {
    uint16_t interval = SwingInterval(channel);
    return (elapsed >= interval &&
      elapsed <= interval + kTimingErrorCorrectionAmount);
  } else {
    // thru
    return false;
  }
}

void SwingReset(uint8_t channel) {
  swing_counter[channel] = 0;
}

void SwingHandleInputGateRisingEdge(uint8_t channel) {
  switch (swing_counter[channel]) {
    case 0: // thru beat
            last_thru_at[channel] = TCNT1;
            last_output_at[channel] = TCNT1;
            ++swing_counter[channel];
            break;
    case 1: // skipped thru beat
            // unless lowest setting, no swing - should do thru
            if (swing[channel] <= kSwingFactorMin) {
              last_output_at[channel] = TCNT1;
            } // else rest
            ++swing_counter[channel];
            break;
    default: SwingReset(channel); // something is wrong if we're here so reset
             break;
  }
}

inline void SwingExec(uint8_t channel) {
  if (SwingShouldExec(channel, PulseTrackerGetElapsed(channel))) {
    last_output_at[channel] = TCNT1;
    SwingReset(channel);
  }
}

void SystemHandleNewAdcValue(uint8_t channel) {
  switch(function_table_[channel]) {
    case CHANNEL_FUNCTION_FACTORER: factor[channel] = FactorGet(channel);
                                    break;
    case CHANNEL_FUNCTION_SWING: swing[channel] = SwingGet(channel);
                                 break;
  }
}

inline void SystemExec(uint8_t channel) {
  switch(function_table_[channel]) {
    case CHANNEL_FUNCTION_FACTORER: MultiplyExec(channel);
                                    break;
    case CHANNEL_FUNCTION_SWING: SwingExec(channel);
                                 break;
  }

  // Do stuff
  if (last_output_at[channel] == TCNT1) {
    GateOutputOn(channel);
    last_thru_at[channel] == TCNT1 ? LedExecThru(channel) : LedExecFactored(channel);
  } else {
    GateOutputOff(channel);
  }
}

inline void SystemHandleInputGateRisingEdge(uint8_t channel) {
  // Pulse tracker is always recording. this should help smooth transitions
  // between functions
  PulseTrackerRecord(channel);

  switch(function_table_[channel]) {
    case CHANNEL_FUNCTION_FACTORER: FactorerHandleInputGateRisingEdge(channel);
                                    break;
    case CHANNEL_FUNCTION_SWING: SwingHandleInputGateRisingEdge(channel);
                                    break;
  }
}

inline void ChannelExec(uint8_t channel) {
  if (AdcHasNewValue(channel)) {
    SystemHandleNewAdcValue(channel);
  }

  if (GateInputIsRisingEdge(channel)) {
    SystemHandleInputGateRisingEdge(channel);
  }

  SystemExec(channel);
  LedsUpdate(channel);
}

inline void Loop() {

  AdcScan();

  if (ClockIsOverflow()) {
    ClockHandleOverflow();
  }

  for (uint8_t i = 0; i < kSystemNumChannels; ++i) {
    ChannelExec(i);
  }
}

int main(void) {

  ResetWatchdog();
  Init();

  while (1) {
    Loop();
  }

  return 0;
}
