#include <vector>
#include <map>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "EVXTimer.h"
#include "EVXLEDController.h"
#include "EVXAudioRecorder.h"
#include "biquad.h"
#include "notch.h"
#include "ramp.h"
#include "blinky_functions.h"
#include "dip_switch.h"

#include "config.h"

// Here we define for convenience an array of LEDs GPIO
// and an enum to keep track of which is which
// The ordering should correspond
const vector<int> leds{LED_RED, LED_WHITE, LED_BLUE, LED_GREEN};
map<int, uint32_t> duty_max {
  {LED_RED, 1200},
  {LED_WHITE, 1200},
  {LED_BLUE, 1000},
  {LED_GREEN, 4095}
};

// States
enum class State : char
{
  WHITE_SIG_NO_REF = 0,
  CALIBRATION = 1,
  RED_SIG_BLUE_REF = 2,
  RED_BLUE_DOUBLE_REF = 3,
  CALIBRATION_MAP = 4,
  STATE_5 = 5,
  STATE_6 = 6,
  STATE_7 = 7
};
State state_current = State::WHITE_SIG_NO_REF;

// For the calibration
int counter = 0;
int current_led = LED_WHITE;

// DC removal filter
DCRemoval dc_rm(DC_REMOVAL_ALPHA);

void main_process()
{

    // The maximum duty cycle of blue and red was set so that
    // there is no saturation in the video recording
    // The reference is set to half of the maximum
    map<int, uint32_t> duty_ref;
    for (auto p: duty_max)
      duty_ref[p.first] = p.second / 2;

    // We use a ramp signal for calibration
    Ramp the_ramp(1. / CALIB_PERIOD_SEC, 0.);

    // Configure dip switch and read current state
    DipSwitch3 dip_switch(DIP_SWITCH_1, DIP_SWITCH_2, DIP_SWITCH_3);


    // for measuring elapsed time.
    EVXTimer* timer = new EVXTimer();
    // for controlling LEDs.
    EVXLEDController* ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);
    // for recording.
    EVXAudioRecorder* recorder = new EVXAudioRecorder(SAMPLE_RATE, AUDIO_BUFFER_SIZE, I2S_BCK, I2S_WS, I2S_DATA_IN, CPU_NUMBER);
    
    // audio record start
    recorder->start();

    float elapsed_time;

    while (1) {
        
        // Get the audio data.
        // This function waits until buffer is accumulated
        // audio_data is interleaved data. (length:AUDIO_BUFFER_SIZE x AUDIO_CHANNELS)
        float* audio_data = recorder->wait_for_buffer_to_accumulate();

        State state_new = (State)dip_switch.read();

        switch (state_new)
        {


          case State::RED_BLUE_DOUBLE_REF:
            if (state_new != state_current)
            {
              // Turn off all LEDs
              for (int i=0 ; i < 4 ; i++)
                ledC->updateDuty(leds[i], 0);
              // Use the Blue LED as reference at half PWM resolution
              ledC->updateDuty(LED_RED, duty_ref[LED_RED]);
              ledC->updateDuty(LED_BLUE, duty_ref[LED_BLUE]);
              state_current = state_new;
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
            break;

          case State::CALIBRATION:
          case State::CALIBRATION_MAP:
            {
              if (state_new != state_current)
              {
                // do some initialization
                current_led = LED_RED;
                state_current = state_new;

                // Turn off all LEDs
                for (int i=0 ; i < 4 ; i++)
                  ledC->updateDuty(leds[i], 0);

                // Reset the ramp function
                the_ramp.reset(0.);
                timer->start();
              }
              else
              {
                float now = timer->measure();
                the_ramp.update(now * 1e-3);
              }

              float duty_f = the_ramp.get_value();

              if (duty_f >= 1.)
              {
                ledC->updateDuty(current_led, 0);

                if (current_led == LED_RED)
                  current_led = LED_BLUE;  // red -> blue
                else
                  current_led = LED_RED;  // blue -> red

                // Reset the function
                the_ramp.reset(0.);
                timer->start();
                duty_f = the_ramp.get_value();
              }

              // Optionally apply mapping depending on state
              if (state_current == State::CALIBRATION_MAP)
                duty_f = blinky_non_linearity(duty_f);

              uint32_t duty = duty_f * duty_max[current_led];
              ledC->updateDuty(current_led, duty);

              vTaskDelay(10 / portTICK_PERIOD_MS);
            }

            break;

          case State::WHITE_SIG_NO_REF:
          case State::RED_SIG_BLUE_REF:
            {
              if (state_new != state_current)
              {
                state_current = state_new;

                // Turn off all LEDs
                for (int i = 0 ; i < leds.size() ; i++)
                  ledC->updateDuty(leds[i], 0);

                // Use the Blue LED as reference at half PWM resolution
                if (state_new == State::RED_SIG_BLUE_REF)
                  ledC->updateDuty(LED_BLUE, duty_ref[LED_BLUE]);

                counter = 0;
              }

              timer->start();
              // Only single channel processing
              for (int n=0; n<1; n++) {

                float filter_output = 0.;
                for (int i = 0 ; i < AUDIO_BUFFER_SIZE ; i++) {
                  // remove the mean using a notch filter
                  float sample = dc_rm.process(audio_data[AUDIO_CHANNELS * i + n]);
                  // square to compute the power and accumulate
                  filter_output += sample * sample;
                }
                // divide to obtain mean power in the current buffer
                filter_output /= AUDIO_BUFFER_SIZE;

                // Apply the non-linear transformation
                float val_db = decibels(filter_output);
                float duty_f = map_to_unit_interval(val_db, MIN_DB, MAX_DB);
                duty_f = blinky_non_linearity(duty_f);  // this will also clip in [0, 1]

                // Detect if the signal is too large
                if (duty_f >= 1.)
                  ledC->updateDuty(leds[LED_GREEN], 400);
                else
                  ledC->updateDuty(leds[LED_GREEN], 0);

                // Set the LED duty cycle
                uint32_t duty = 0;
                if (state_current == State::RED_SIG_BLUE_REF)
                {
                  duty = (uint32_t)(duty_f * duty_max[LED_RED]);
                  ledC->updateDuty(LED_RED, duty);
                }
                else if (state_current == State::WHITE_SIG_NO_REF)
                {
                  duty = (uint32_t)(duty_f * duty_max[LED_WHITE]);
                  ledC->updateDuty(LED_WHITE, duty);
                }

                if (ENABLE_MONITOR and counter % 50 == 0)
                {
                  printf("power_val=%e db=%d duty_f=%e duty=%d dip_val=%d\n",
                      (double)filter_output, (int)val_db, (double)duty_f, (int)duty, (int)dip_switch.read());
                }
                counter += 1;

              }

              elapsed_time = timer->measure();
              if (elapsed_time > (float)AUDIO_BUFFER_SIZE/(float)SAMPLE_RATE*1000.0f)
              {
                // elapsed_time must be less than AUDIO_BUFFER_SIZE/SAMPLE_RATE*1000(msec)
                printf(
                    "elapsed_time must be less than %f(msec)\n",
                    (float)AUDIO_BUFFER_SIZE/(float)SAMPLE_RATE*1000.0f
                );
              }

            }
            break;

          case State::STATE_5:
          case State::STATE_6:
          case State::STATE_7:
            break;

        }
    }
}
