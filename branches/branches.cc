//
// VC Clock div/mult
// Alternate firmware for Mutable Instruments Branches
//
// Based on code from the original Mutable Instruments Branches firmware
// Branches is Copyright 2012 Olivier Gillet.
//

//#include <avr/eeprom.h>
//#include <avr/pgmspace.h>

#include "avrlib/adc.h"
#include "avrlib/boot.h"
#include "avrlib/gpio.h"
#include "avrlib/watchdog_timer.h"

using namespace avrlib;

#define SYSTEM_NUM_CHANNELS 2
#define SYSTEM_NUM_GATE_INPUTS 1

enum ChannelMode {
  CHANNEL_FUNCTION_FACTORER,
  CHANNEL_FUNCTION_SWING,
  CHANNEL_FUNCTION_LAST
};

const ChannelMode function_table_[SYSTEM_NUM_CHANNELS] = {
  CHANNEL_FUNCTION_SWING,
  CHANNEL_FUNCTION_FACTORER
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

//#define BUTTON_LONG_PRESS_TIME 6250  // 800 * 8000 / 1024
#define LED_THRU_GATE_DURATION 0x100
#define LED_FACTORED_GATE_DURATION 0x080
#define LED_READY_GATE_DURATION 0x0FF

#define PULSE_TRACKER_BUFFER_SIZE 2
#define FUNCTION_TIMING_ERROR_CORRECTION_AMOUNT 12
#define ADC_POLL_RATIO 5 // 1:5
#define ADC_DELTA_THRESHOLD 4
#define SWING_FACTOR_MIN 50
#define SWING_FACTOR_MAX 70
#define FACTORER_NUM_FACTORS 15
#define FACTORER_BYPASS_INDEX 7
#define ADC_MAX_VALUE 250
// Top input must be reset since they are hardware normaled
#define GATE_INPUT_RESET_INDEX 0
#define GATE_INPUT_TRIG_INDEX 1

static uint8_t adc_channel;

bool gate_input_state[SYSTEM_NUM_GATE_INPUTS];
//bool switch_state[SYSTEM_NUM_CHANNELS];
//bool inhibit_switch[SYSTEM_NUM_CHANNELS];

//uint16_t press_time[SYSTEM_NUM_CHANNELS];
uint8_t led_state[SYSTEM_NUM_CHANNELS];
uint16_t led_gate_duration[SYSTEM_NUM_CHANNELS];

uint16_t pulse_tracker_buffer[PULSE_TRACKER_BUFFER_SIZE];

bool multiply_is_debouncing[SYSTEM_NUM_CHANNELS];

int8_t divide_counter[SYSTEM_NUM_CHANNELS];
int8_t swing_counter[SYSTEM_NUM_CHANNELS];

uint16_t last_output_at[SYSTEM_NUM_CHANNELS];
uint8_t exec_state[SYSTEM_NUM_CHANNELS];

uint8_t adc_counter;
int16_t adc_value[SYSTEM_NUM_CHANNELS];

int16_t factor[SYSTEM_NUM_CHANNELS];
int16_t swing[SYSTEM_NUM_CHANNELS];

void GateInputsInit() {

  in_1.set_mode(DIGITAL_INPUT);
  in_1.High();
  gate_input_state[0] = false;

  in_2.set_mode(DIGITAL_INPUT);
  in_2.High();
  gate_input_state[1] = false;
}
/*
void SwitchesInit() {
  switch_1.set_mode(DIGITAL_INPUT);
  switch_2.set_mode(DIGITAL_INPUT);
  switch_1.High();
  switch_2.High();

  switch_state[0] = switch_state[1] = false;
}
*/
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
  adc.set_num_inputs(2);
  Adc::set_reference(ADC_DEFAULT);
  Adc::set_alignment(ADC_LEFT_ALIGNED);
  adc_counter = 1;
}

void SystemInit() {
  Gpio<PortB, 4>::set_mode(DIGITAL_OUTPUT);
  Gpio<PortB, 4>::Low();

  GateInputsInit();
  //SwitchesInit();
  GateOutputsInit();
  LedsInit();
  AdcInit();

  TCCR1A = 0;
  TCCR1B = 5;

  //FactorInit();
}

/*
void SystemDisplayReady() {
  for (uint8_t i; i < SYSTEM_NUM_CHANNELS; ++i) {
    led_gate_duration[i] = LED_READY_GATE_DURATION;
    led_state[i] = LED_STATE_GREEN;
  }
}*/

inline bool GateInputRead(uint8_t channel) {
  //return !in_1.value();
  return channel == 0 ? !in_1.value() : !in_2.value();
}

/*bool SwitchRead(uint8_t channel) {
  return channel == 0 ? !switch_1.value() : !switch_2.value();
}*/

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

inline void PulseTrackerClear() {
  pulse_tracker_buffer[PULSE_TRACKER_BUFFER_SIZE-2] = 0;
  pulse_tracker_buffer[PULSE_TRACKER_BUFFER_SIZE-1] = 0;
}

inline uint16_t PulseTrackerGetElapsed() {
  return TCNT1 - pulse_tracker_buffer[PULSE_TRACKER_BUFFER_SIZE - 1];
}

// The period of time between the last two recorded events
inline uint16_t PulseTrackerGetPeriod() {
  return pulse_tracker_buffer[PULSE_TRACKER_BUFFER_SIZE - 1] - pulse_tracker_buffer[PULSE_TRACKER_BUFFER_SIZE - 2];
}

void PulseTrackerRecord() {
  // shift
  pulse_tracker_buffer[PULSE_TRACKER_BUFFER_SIZE - 2] = pulse_tracker_buffer[PULSE_TRACKER_BUFFER_SIZE - 1];
  pulse_tracker_buffer[PULSE_TRACKER_BUFFER_SIZE - 1] = TCNT1;
}

// Is the factor control setting such that we're in multiplier mode?
inline bool MultiplyIsEnabled(uint8_t channel) {
  return factor[channel] < 0;
}

// Is the pulse tracker populated with enough events to perform multiply?
inline bool MultiplyIsPossible(uint8_t channel) {
  return pulse_tracker_buffer[PULSE_TRACKER_BUFFER_SIZE - 1] > 0 &&
    pulse_tracker_buffer[PULSE_TRACKER_BUFFER_SIZE - 2] > 0;
}

// The time interval between multiplied events
// eg if clock is comes in at 100 and 200, and the clock multiply factor is 2,
// the result will be 50
inline uint16_t MultiplyInterval(uint8_t channel) {
  return PulseTrackerGetPeriod() / -factor[channel];
}

inline bool MultiplyShouldExec(uint8_t channel, uint16_t elapsed) {
  uint16_t interval = MultiplyInterval(channel);
  if (elapsed % interval <= FUNCTION_TIMING_ERROR_CORRECTION_AMOUNT) {
    if (!multiply_is_debouncing[channel]) {
      return true;
    }
    // debounce window/return false
  } else {
    // debounce is finished
    multiply_is_debouncing[channel] = false;
  }
  return false;
}

// Is the factor control setting such that we're in divider mode?
inline bool DivideIsEnabled(uint8_t channel) {
  return factor[channel] > 0;
}

inline bool DivideShouldExec(uint8_t channel) {
  return divide_counter[channel] <= 0;
}

inline int16_t FactorGet(uint8_t channel) {
  int16_t factor_index = (adc_value[channel] / (ADC_MAX_VALUE / (FACTORER_NUM_FACTORS - 1))) - FACTORER_BYPASS_INDEX;
  // offset result so that there's no -1 or 1 factor, but values are still evenly spaced
  if (factor_index == 0) {
    return 0;
  } else if (factor_index < 0) {
    return --factor_index;
  } else {
    return ++factor_index;
  }
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

inline void AdcScan() {
  if (adc_counter == (ADC_POLL_RATIO-1)) {
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
    int16_t delta = value - adc_value[channel];
    // abs
    if (delta < 0) {
      delta = -delta;
    }
    if (delta > ADC_DELTA_THRESHOLD) {
      // store factor control value
      adc_value[channel] = ADC_MAX_VALUE - value;
      if (adc_value[channel] < 0) {
        adc_value[channel] = 0;
      } else if (adc_value[channel] > ADC_MAX_VALUE) {
        adc_value[channel] = ADC_MAX_VALUE;
      }
      return true;
    }
  }
  return false;
}

inline bool ClockIsOverflow() {
  return (last_output_at[0] > TCNT1 || last_output_at[1] > TCNT1);
}

inline void ClockHandleOverflow() {
  PulseTrackerClear();
  for (uint8_t i = 0; i < SYSTEM_NUM_CHANNELS; ++i) {
    last_output_at[i] = 0;
  }
}

inline void LedExecThru(uint8_t channel) {
  led_gate_duration[channel] = LED_THRU_GATE_DURATION;
  led_state[channel] = 1;
}

inline void LedExecFactored(uint8_t channel) {
  led_gate_duration[channel] = LED_FACTORED_GATE_DURATION;
  led_state[channel] = 2;
}

inline int8_t SwingGet(uint8_t channel) {
  return (adc_value[channel] / (ADC_MAX_VALUE / (SWING_FACTOR_MAX - SWING_FACTOR_MIN))) + SWING_FACTOR_MIN;
}

inline void LedUpdate(uint8_t channel) {
  //
  if (led_gate_duration[channel]) {
    --led_gate_duration[channel];
    if (!led_gate_duration[channel]) {
      led_state[channel] = 0;
    }
  }

  // Update Leds
  switch (led_state[channel]) {
    case 0: LedOff(channel);
            break;
    case 1: LedGreen(channel);
            break;
    case 2: LedRed(channel);
            break;
  }
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
        MultiplyShouldExec(channel, PulseTrackerGetElapsed())) {
    last_output_at[channel] = TCNT1;
    exec_state[channel] = 2;
    multiply_is_debouncing[channel] = true;
  }
}

inline void DivideReset(uint8_t channel) {
  divide_counter[channel] = 0;
}

inline bool DivideShouldReset(uint8_t channel) {
  return divide_counter[channel] >= (factor[channel] - 1);
}

void FactorerHandleInputGateRisingEdge(uint8_t channel) {
  //
  if (DivideIsEnabled(channel)) {
    if (DivideShouldExec(channel)) {
      last_output_at[channel] = TCNT1;
      exec_state[channel] = 2; // divide converts thru to exec on every division
    }
    // deal with counter
    if (DivideShouldReset(channel)) {
      DivideReset(channel);
    } else {
      ++divide_counter[channel];
    }
  } else {
    // all thru
    exec_state[channel] = 1; // mult always acknowledges thru
    last_output_at[channel] = TCNT1;
  }
}

inline uint16_t SwingInterval(uint8_t channel) {
  uint16_t period = PulseTrackerGetPeriod();
  return ((10 * (period * 2)) / (1000 / swing[channel])) - period;
}

inline bool SwingShouldExec(uint8_t channel, uint16_t elapsed) {
  if (swing_counter[channel] >= 2 && swing[channel] > SWING_FACTOR_MIN) {
    uint16_t interval = SwingInterval(channel);
    return (elapsed >= interval &&
      elapsed <= interval + FUNCTION_TIMING_ERROR_CORRECTION_AMOUNT);
  } else {
    // thru
    return false;
  }
}

inline void SwingReset(uint8_t channel) {
  swing_counter[channel] = 0;
}

inline void SwingExecThru(uint8_t channel) {
  exec_state[channel] = 1;
  last_output_at[channel] = TCNT1;
}

inline void SwingStrike(uint8_t channel) {
  exec_state[channel] = 2;
  last_output_at[channel] = TCNT1;
}

void SwingHandleInputGateRisingEdge(uint8_t channel) {
  switch (swing_counter[channel]) {
    case 0: // thru beat
            SwingExecThru(channel);
            swing_counter[channel] = 1;
            break;
    case 1: // skipped thru beat
            // unless lowest setting, no swing - should do thru
            if (swing[channel] <= SWING_FACTOR_MIN) {
              SwingStrike(channel);
              SwingReset(channel);
            } else {
              // rest
              exec_state[channel] = 0;
              swing_counter[channel] = 2;
            }
            break;
    default: SwingReset(channel); // something is wrong if we're here so reset
             break;
  }
}

inline void SwingExec(uint8_t channel) {
  if (SwingShouldExec(channel, PulseTrackerGetElapsed())) {
    SwingStrike(channel);
    SwingReset(channel); // reset
  }
}

inline void SystemHandleNewAdcValue(uint8_t channel) {
  switch(function_table_[channel]) {
    case CHANNEL_FUNCTION_FACTORER: factor[channel] = FactorGet(channel);
                                    break;
    case CHANNEL_FUNCTION_SWING: swing[channel] = SwingGet(channel);
                                 break;
  }
}

inline void FunctionExec(uint8_t channel) {
  switch(function_table_[channel]) {
    case CHANNEL_FUNCTION_FACTORER: MultiplyExec(channel);
                                    break;
    case CHANNEL_FUNCTION_SWING: SwingExec(channel);
                                 break;
  }

  // Do stuff
  if (exec_state[channel] > 0) {
    GateOutputOn(channel);
    (exec_state[channel] < 2) ? LedExecThru(channel) : LedExecFactored(channel);
  } else {
    GateOutputOff(channel);
  }
  exec_state[channel] = 0; // clean up
}

inline void FunctionReset(uint8_t channel) {
  switch(function_table_[channel]) {
    case CHANNEL_FUNCTION_FACTORER: DivideReset(channel);
                                    break;
    case CHANNEL_FUNCTION_SWING: SwingReset(channel);
                                 break;
  }
}

inline void FunctionHandleInputGateRisingEdge(uint8_t channel) {
  switch(function_table_[channel]) {
    case CHANNEL_FUNCTION_FACTORER: FactorerHandleInputGateRisingEdge(channel);
                                    break;
    case CHANNEL_FUNCTION_SWING: SwingHandleInputGateRisingEdge(channel);
                                    break;
  }
}

inline void ChannelExec(uint8_t channel) {
  // do stuff
  FunctionExec(channel);
  LedUpdate(channel);
}

inline void SystemHandleInput(uint8_t channel, bool is_trig, bool is_reset) {
  // handle pot/cv in
  if (AdcHasNewValue(channel)) {
    SystemHandleNewAdcValue(channel);
  }
  // handle clock/trig/gate input
  if (is_trig) {
    FunctionHandleInputGateRisingEdge(channel);
  }
  // handle reset input
  if (is_reset) {
    FunctionReset(channel);
  }
}

inline void Loop() {

  if (ClockIsOverflow()) {
    ClockHandleOverflow();
  }

  // scan pot/cv in
  AdcScan();

  // scan clock/trig/gate input
  bool is_trig = false;

  if (GateInputIsRisingEdge(GATE_INPUT_TRIG_INDEX)) {
    // Pulse tracker is always recording. this should help smooth transitions
    // between functions even though divide doesn't use it
    PulseTrackerRecord();
    is_trig = true;
  }

  // scan reset input
  bool is_reset = GateInputIsRisingEdge(GATE_INPUT_RESET_INDEX);

  // do stuff
  for (uint8_t i = 0; i < SYSTEM_NUM_CHANNELS; ++i) {
    SystemHandleInput(i, is_trig, is_reset);
    ChannelExec(i);
  }
}

int main(void) {
  ResetWatchdog();
  SystemInit();

  while (1) {
    Loop();
  }
}
