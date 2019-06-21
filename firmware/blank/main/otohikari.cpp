#include <vector>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "EVXTimer.h"
#include "EVXLEDController.h"
#include "EVXAudioRecorder.h"
#include "DIPSwitch.h"

// LED
#define LED_GREEN (25) // Left  Bottom
#define LED_BLUE (27)  // Left  Top
#define LED_RED (26)   // Right Bottom
#define LED_WHITE (15) // Right Top
#define LED_LEFT LED_RED  // DAC left channel
#define LED_RIGHT LED_GREEN  // DAC right channel
#define LED_RESOLUTION LEDC_TIMER_12_BIT
#define LED_FREQUENCY (16000)

// DIP SWITCHES
#define DIP_SWITCH_1 GPIO_NUM_13
#define DIP_SWITCH_2 GPIO_NUM_32
#define DIP_SWITCH_3 GPIO_NUM_33
#define DIP_SWITCH_INPUT_PIN_SEL ((1ULL<<DIP_SWITCH_1) | (1ULL<<DIP_SWITCH_2) | (1ULL<<DIP_SWITCH_3))

// AUDIO
#define SAMPLE_RATE (16000)
#define AUDIO_BUFFER_SIZE (64)
#define I2S_BCK (23)
#define I2S_WS (14)
#define I2S_DATA_IN (22)
#define CPU_NUMBER (0)

void main_process()
{

  // Configure dip switch and read current state
  DIPSwitch dipswitch(DIP_SWITCH_1, DIP_SWITCH_2, DIP_SWITCH_3);

  vector<int> leds{LED_RED, LED_WHITE, LED_BLUE, LED_GREEN};

  // for measuring elapsed time.
  EVXTimer* timer = new EVXTimer();

  // for controlling LEDs.
  EVXLEDController* ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);

  // for recording.
  EVXAudioRecorder* recorder = new EVXAudioRecorder(SAMPLE_RATE, AUDIO_BUFFER_SIZE, I2S_BCK, I2S_WS, I2S_DATA_IN, CPU_NUMBER);

  uint32_t duty_max = (uint32_t)powf(2.0f, LED_RESOLUTION);

  // audio record start
  recorder->start();

  // start the timing
  timer->start();

  // This the main loop. The audio will be read in chunks of AUDIO_BUFFER_SIZE samples
  // and some processing can be done.
  while (1) {

    // Get the audio data.
    // This function waits until buffer is accumulated
    // audio_data is interleaved data. (length:AUDIO_BUFFER_SIZE x 2)
    float* audio_data = recorder->wait_for_buffer_to_accumulate();

    // We can read the state of the switch and take different actions depending
    // on its value. The switch can take 8 different values.
    uint8_t state = dipswitch.read();

    // Here we do some example processing on the audio data. We will
    // find the maximum power in the block and light up the LED if it
    // Only single channel processing
    
    // Keep track of max power in this buffer
    float max_pwr = -10000.;

    // Loop over samples, two at a time because of two channels
    for (int n = 0; n < AUDIO_BUFFER_SIZE; n += AUDIO_CHANNELS)
    {
      for (int ch = 0 ; ch < AUDIO_CHANNELS ; ch++)
      {

        // This is the current sample
        float sample = audio_data[ch + n];

        // Compute the power of the audio sample in dB
        float sample_db = 10. * log10f(sample * sample);

        if (sample_db > max_pwr)
          max_pwr = sample_db;

        // Turn on the LED (at PWM max duty cycle, i.e. continuously ON)
        // if power is larger than 70 decibels
        if (sample_db > -40)
          ledC->updateDuty(leds[0], duty_max);
        else
          ledC->updateDuty(leds[0], 0.);
      }
    }

    // We should not do too many printf in a tight loop so
    // we restrict it to once every second
    if (timer->measure() > 1000.0f) {
      printf("State: %d Power: %f\n", (int)state, max_pwr);
      timer->start();
    }
  }
}
