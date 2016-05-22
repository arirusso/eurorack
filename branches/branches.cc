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

Adc adc;

const uint16_t kLongPressTime = 6250;  // 800 * 8000 / 1024
const uint16_t kLedClockGateDelay = 0x100;
const uint16_t kLedFactoredGateDelay = 0x080;

static uint8_t adc_channel;

bool input_state[2];
bool switch_state[2];
bool inhibit_switch[2];

uint16_t press_time[2];
uint8_t led_state[2];
uint16_t led_gate_delay[2];

const uint8_t kFactorScaleFactor = 4095;
const uint8_t kClockBufferSize = 10;
const bool kIsLocking = false;

uint16_t factor_control[2];
uint16_t last_factor_control[2];

uint16_t clock[2][kClockBufferSize];
int8_t clock_factor[2];
int8_t div_counter[2];
uint16_t last_factored_time[2];
uint16_t period_[2];

const prog_uint16_t linear_table[] PROGMEM = {
      0,     0,   259,   518,   777,  1036,  1295,  1554,
   1813,  2072,  2331,  2590,  2849,  3108,  3367,  3626,
   3885,  4145,  4404,  4663,  4922,  5181,  5440,  5699,
   5958,  6217,  6476,  6735,  6994,  7253,  7512,  7771,
   8030,  8289,  8548,  8807,  9066,  9325,  9584,  9843,
  10102, 10361, 10620, 10879, 11138, 11397, 11656, 11915,
  12174, 12434, 12693, 12952, 13211, 13470, 13729, 13988,
  14247, 14506, 14765, 15024, 15283, 15542, 15801, 16060,
  16319, 16578, 16837, 17096, 17355, 17614, 17873, 18132,
  18391, 18650, 18909, 19168, 19427, 19686, 19945, 20204,
  20463, 20723, 20982, 21241, 21500, 21759, 22018, 22277,
  22536, 22795, 23054, 23313, 23572, 23831, 24090, 24349,
  24608, 24867, 25126, 25385, 25644, 25903, 26162, 26421,
  26680, 26939, 27198, 27457, 27716, 27975, 28234, 28493,
  28753, 29012, 29271, 29530, 29789, 30048, 30307, 30566,
  30825, 31084, 31343, 31602, 31861, 32120, 32379, 32638,
  32897, 33156, 33415, 33674, 33933, 34192, 34451, 34710,
  34969, 35228, 35487, 35746, 36005, 36264, 36523, 36782,
  37042, 37301, 37560, 37819, 38078, 38337, 38596, 38855,
  39114, 39373, 39632, 39891, 40150, 40409, 40668, 40927,
  41186, 41445, 41704, 41963, 42222, 42481, 42740, 42999,
  43258, 43517, 43776, 44035, 44294, 44553, 44812, 45072,
  45331, 45590, 45849, 46108, 46367, 46626, 46885, 47144,
  47403, 47662, 47921, 48180, 48439, 48698, 48957, 49216,
  49475, 49734, 49993, 50252, 50511, 50770, 51029, 51288,
  51547, 51806, 52065, 52324, 52583, 52842, 53101, 53361,
  53620, 53879, 54138, 54397, 54656, 54915, 55174, 55433,
  55692, 55951, 56210, 56469, 56728, 56987, 57246, 57505,
  57764, 58023, 58282, 58541, 58800, 59059, 59318, 59577,
  59836, 60095, 60354, 60613, 60872, 61131, 61390, 61650,
  61909, 62168, 62427, 62686, 62945, 63204, 63463, 63722,
  63981, 64240, 64499, 64758, 65017, 65276, 65535, 65535,
};

void Init() {
  Gpio<PortB, 4>::set_mode(DIGITAL_OUTPUT);
  Gpio<PortB, 4>::Low();

  in_1.set_mode(DIGITAL_INPUT);
  in_2.set_mode(DIGITAL_INPUT);
  in_1.High();
  in_2.High();

  switch_1.set_mode(DIGITAL_INPUT);
  switch_2.set_mode(DIGITAL_INPUT);
  switch_1.High();
  switch_2.High();

  out_1_a.set_mode(DIGITAL_OUTPUT);
  out_1_b.set_mode(DIGITAL_OUTPUT);
  led_1_a.set_mode(DIGITAL_OUTPUT);
  led_1_k.set_mode(DIGITAL_OUTPUT);

  out_2_a.set_mode(DIGITAL_OUTPUT);
  out_2_b.set_mode(DIGITAL_OUTPUT);
  led_2_a.set_mode(DIGITAL_OUTPUT);
  led_2_k.set_mode(DIGITAL_OUTPUT);

  led_1_a.Low();
  led_2_a.Low();
  led_1_k.Low();
  led_2_k.Low();

  adc.Init();
  adc.set_reference(ADC_DEFAULT);
  adc.set_alignment(ADC_LEFT_ALIGNED);
  adc.StartConversion(0);

  led_state[0] = led_state[1] = 0;
  switch_state[0] = switch_state[1] = false;

  TCCR1A = 0;
  TCCR1B = 5;
}

bool Read(uint8_t channel) {
  return channel == 0 ? !in_1.value() : !in_2.value();
}

bool ReadSwitch(uint8_t channel) {
  return channel == 0 ? !switch_1.value() : !switch_2.value();
}

void GateOn(uint8_t channel) {
  if (channel == 0) {
    out_1_a.High();
    out_1_b.High();
  } else {
    out_2_a.High();
    out_2_b.High();
  }
}

void GateOff(uint8_t channel) {
  if (channel == 0) {
    out_1_a.Low();
    out_1_b.Low();
  } else {
    out_2_a.Low();
    out_2_b.Low();
  }
}

void LedOff(uint8_t channel) {
  if (channel == 0) {
    led_1_a.Low();
    led_1_k.Low();
  } else {
    led_2_a.Low();
    led_2_k.Low();
  }
}

void ClearClockBuffer(uint8_t channel) {
  for (uint8_t i = 0; i < kClockBufferSize; i++) {
    clock[channel][i] = 0;
  }
}

void RecordClock(uint8_t channel) {
  if (clock[channel][kClockBufferSize - 1] != TCNT1) {
    period_[channel] = 0;
    // shift
    for (uint8_t i = kClockBufferSize-1; i > 0; i--) {
      if (clock[channel][i] > 0) {
        clock[channel][i-1] = clock[channel][i];
      }
    }
    // record
    clock[channel][kClockBufferSize - 1] = TCNT1;
  }
}

// Has enough clock been recorded that it's possible to div/mult ?
bool IsClockMultipliable(uint8_t channel) {
  return (clock[channel][kClockBufferSize - 1] > 0 && clock[channel][kClockBufferSize - 2] > 0);
}

uint16_t GetPeriod(uint8_t channel) {
  if (period_[channel] == 0) {
    period_[channel] = clock[channel][kClockBufferSize - 1] - clock[channel][kClockBufferSize - 2];
  }
  return period_[channel];
}

// Compute the period between non-clock outputs
uint16_t Interval(uint8_t channel) {
  // what was the last recorded period ?
  uint16_t period = GetPeriod(channel);

  if (clock_factor[channel] == 0) {
    return period;
  } else if (clock_factor[channel] > 0) {
    return period * clock_factor[channel];
  } else {
    return period / (clock_factor[channel] * -1);
  }
}

void CalculateFactor(uint8_t channel) {
  int16_t bipolar = factor_control[channel] - 32767;
  clock_factor[channel] = (bipolar == 0) ? 0 : bipolar / kFactorScaleFactor;
  //clock_factor[channel] = 2;
  for (uint8_t i = 0; i < 17; i++) {
    uint16_t scaled = 65535 - (4096 * i) || 0;
    if (factor_control[channel] >= scaled) {
      clock_factor[channel] = scaled - 8;
    } else {
      break;
    }
  }
}

int main(void) {

  ResetWatchdog();
  Init();

  input_state[0] = input_state[1] = false;
  while (1) {

    // Scan VC/Pot
    if (adc.ready()) {
      uint8_t channel_index = 1 - adc_channel;  // ADC pins are swapped!
      // record last position
      last_factor_control[channel_index] = factor_control[channel_index];
      // update current position
      factor_control[channel_index] = pgm_read_word(linear_table + adc.ReadOut8());
      adc_channel = (adc_channel + 1) & 1;
      adc.StartConversion(adc_channel);
    }

    // Scan inputs
    for (uint8_t i = 0; i < 2; ++i) {
      bool new_input_state = Read(i);
      bool is_clock = false;
      bool should_output = false;

      if (new_input_state && !input_state[i] /* Rising edge */) {
        // has the factor control changed
        //if (last_factor_control[i] == factor_control[i]) {
          CalculateFactor(i);
        //}
        RecordClock(i);
        // always output on clock input
        is_clock = true;
        should_output = true;

        // clock divider
        if (clock_factor[i] > 0) {
          if (div_counter[i] == clock_factor[i]) {
            is_clock = false;
            div_counter[i] = 0;
          } else {
            div_counter[i]++;
          }
        }
      }

      // check if it should output multiplied clock
      if (clock_factor[i] < 0 && IsClockMultipliable(i)) {
        // look for clock overflow
        if (last_factored_time[i] > TCNT1) {
          ClearClockBuffer(i);
          period_[i] = 0;
          last_factored_time[i] = 0;
        } else {
          uint16_t elapsed = TCNT1 - last_factored_time[i];
          // is that the current time?
          if (elapsed == Interval(i)) {
            last_factored_time[i] = TCNT1;
            should_output = true;
          }
        }
      }

      // do stuff
      if (should_output) {
        GateOn(i);
        if (is_clock) {
          led_gate_delay[i] = kLedClockGateDelay;
          led_state[i] = 1;
        } else {
          led_gate_delay[i] = kLedFactoredGateDelay;
          led_state[i] = 2;
        }
      } else {
        GateOff(i);
      }
      input_state[i] = new_input_state;

      if (led_gate_delay[i]) {
        --led_gate_delay[i];
        if (!led_gate_delay[i]) {
          led_state[i] = 0;
        }
      }
    }

    // Refresh LEDs
    if (led_state[0] == 0) {
      led_1_a.Low();
      led_1_k.Low();
    } else if (led_state[0] == 1) {
      led_1_a.Low();
      led_1_k.High();
    } else {
      led_1_a.High();
      led_1_k.Low();
    }

    if (led_state[1] == 0) {
      led_2_a.Low();
      led_2_k.Low();
    } else if (led_state[1] == 1) {
      led_2_a.Low();
      led_2_k.High();
    } else {
      led_2_a.High();
      led_2_k.Low();
    }

  }
}
