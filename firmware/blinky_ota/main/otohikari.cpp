#include <stdio.h>
#include <math.h>
#include <map>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "EVXTimer.h"
#include "EVXLEDController.h"
#include "EVXAudioRecorder.h"
#include "EVXGPIOController.h"
#include "biquad.h"
#include "notch.h"
#include "ramp.h"
#include "blinky_functions.h"
#include "dip_switch.h"

#include "config.h"
#include "non_linear_mapping.cpp"

// LED
#define LED_BLUE (27)
#define LED_GREEN (25)
#define LED_WHITE (15)
#define LED_RED (26)
#define LED_RESOLUTION LEDC_TIMER_12_BIT
#define LED_FREQUENCY (1000)

// GPIO
//#define DIP_SWITCH_1 (13)
//#define DIP_SWITCH_2 (32)
//#define DIP_SWITCH_3 (33)

// AUDIO
#define SAMPLE_RATE (48000)
#define AUDIO_BUFFER_SIZE (1024)
#define I2S_BCK (23)
#define I2S_WS (14)
#define I2S_DATA_IN (22)
#define CPU_NUMBER (0)

// LED & AUDIO
#define MIN_DB (-45.0f)
#define MAX_DB (-10.0f)

// States
enum class State : char
{
    WHITE_SIGNAL = 0,
    RED_SIGNAL = 1,
    CALIBRATION_RAMP = 2,
    CALIBRATION_RAMP_MAP = 3,
    RED_BLUE_DOUBLE_REF = 4,
    RED_CALIBRATION = 5,
    WHITE_CALIBRATION = 6,
    STATE_7 = 7
};
State state_current = State::WHITE_SIGNAL;

// For the calibration
int counter = 0;
int current_led = LED_WHITE;

// DC removal filter
DCRemoval dc_rm(DC_REMOVAL_ALPHA);

static void wifi_connected()
{
    vector<int> leds{LED_RED};
    EVXLEDController *ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);

    uint32_t duty_max = (uint32_t)powf(2.0f, LED_RESOLUTION) - 1;
    ledC->updateDuty(leds[0], duty_max);

    vTaskDelete(NULL);
}

static void server_connected()
{
    vector<int> leds{LED_RED, LED_GREEN};
    EVXLEDController *ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);

    uint32_t duty_max = (uint32_t)powf(2.0f, LED_RESOLUTION) - 1;
    ledC->updateDuty(leds[0], duty_max);
    ledC->updateDuty(leds[1], duty_max);

    vTaskDelete(NULL);
}

// for LED animation while downloading
static void download_process()
{
    vector<int> leds{LED_BLUE, LED_WHITE, LED_RED, LED_GREEN};
    EVXLEDController *ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);

    uint32_t duty_max = (uint32_t)powf(2.0f, LED_RESOLUTION) - 1;

    uint8_t rotate = 0;
    while (1)
    {
        for (int i = 0; i < leds.size(); i++)
        {
            if (i == rotate)
            {
                ledC->updateDuty(leds[i], duty_max);
            }
            else
            {
                ledC->updateDuty(leds[i], 0);
            }
        }
        rotate = (rotate + 1) % leds.size();

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// for main process
static void main_process()
{
    vector<int> leds{LED_BLUE, LED_GREEN, LED_WHITE, LED_RED};
    //vector<int> gpios{DIP_SWITCH_1, DIP_SWITCH_2, DIP_SWITCH_3}; // 2^3 = 8 pattern

    map<int, uint32_t> duty_ref;
    for (auto p : duty_max)
        duty_ref[p.first] = p.second / 2;

    // for Brightness of LED
    // +1 for turning off
    //uint32_t duties[AUDIO_CHANNELS + 1];
    //duties[AUDIO_CHANNEL_NONE] = 0;

    // Configure dip switch and read current state
    DipSwitch3 dip_switch(DIP_SWITCH_1, DIP_SWITCH_2, DIP_SWITCH_3);

    // We use a ramp signal for calibration
    Ramp the_ramp(1. / CALIB_PERIOD_SEC, 0.);

    // Pattern of LED that changes according to GPIO state
    vector<vector<int>> led_pattern{
        vector<int>{AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_NONE},   // GPIO switch : 0
        vector<int>{AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_RIGHT},   // GPIO switch : 1
        vector<int>{AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT},  // GPIO switch : 2
        vector<int>{AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_NONE},   // GPIO switch : 3
        vector<int>{AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_LEFT},   // GPIO switch : 4
        vector<int>{AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT},  // GPIO switch : 5
        vector<int>{AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT},    // GPIO switch : 6
        vector<int>{AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT} // GPIO switch : 7
    };

    // for measuring elapsed time.
    EVXTimer *timer = new EVXTimer();
    // for controlling GPIOs.
    //EVXGPIOController *gpioC = new EVXGPIOController(gpios);
    // for controlling LEDs.
    EVXLEDController *ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);
    // for recording.
    EVXAudioRecorder *recorder = new EVXAudioRecorder(SAMPLE_RATE, AUDIO_BUFFER_SIZE, I2S_BCK, I2S_WS, I2S_DATA_IN, CPU_NUMBER);

    //uint32_t duty_max = (uint32_t)powf(2.0f, LED_RESOLUTION) - 1;

    // audio record start
    recorder->start();

    float elapsed_time;

    while (1)
    {

        // Get the audio data.
        // This function waits until buffer is accumulated
        // audio_data is interleaved data. (length:AUDIO_BUFFER_SIZE x AUDIO_CHANNELS)
        float *audio_data = recorder->wait_for_buffer_to_accumulate();

        State state_new = (State)dip_switch.read();

        switch (state_new)
        {

        case State::RED_BLUE_DOUBLE_REF:
            if (state_new != state_current)
            {
                // Turn off all LEDs
                for (int i = 0; i < 4; i++)
                    ledC->updateDuty(leds[i], 0);
                // Use the Blue LED as reference at half PWM resolution
                ledC->updateDuty(LED_RED, duty_ref[LED_RED]);
                ledC->updateDuty(LED_BLUE, duty_ref[LED_BLUE]);
                state_current = state_new;
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
            break;

        case State::CALIBRATION_RAMP:
        case State::CALIBRATION_RAMP_MAP:
        {
            if (state_new != state_current)
            {
                // do some initialization
                current_led = LED_WHITE;
                state_current = state_new;

                // Turn off all LEDs
                for (int i = 0; i < 4; i++)
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

                if (current_led == LED_WHITE)
                    current_led = LED_RED; // red -> blue
                else
                    current_led = LED_WHITE; // blue -> red

                // Reset the function
                the_ramp.reset(0.);
                timer->start();
                duty_f = the_ramp.get_value();
            }

            // Optionally apply mapping depending on state
            if (state_current == State::CALIBRATION_RAMP_MAP)
                duty_f = blinky_non_linearity(duty_f);

            uint32_t duty = duty_f * duty_max[current_led];
            ledC->updateDuty(current_led, duty);

            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        break;

        case State::WHITE_SIGNAL:
        case State::RED_SIGNAL:
        {
            if (state_new != state_current)
            {
                state_current = state_new;

                // Turn off all LEDs
                for (int i = 0; i < leds.size(); i++)
                    ledC->updateDuty(leds[i], 0);

                counter = 0;
            }

            timer->start();
            // Only single channel processing
            for (int n = 0; n < 1; n++)
            {

                float filter_output = 0.;
                for (int i = 0; i < AUDIO_BUFFER_SIZE; i++)
                {
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
                duty_f = blinky_non_linearity(duty_f); // this will also clip in [0, 1]

                // Detect if the signal is too large
                if (duty_f >= 1.)
                    ledC->updateDuty(leds[LED_GREEN], 400);
                else
                    ledC->updateDuty(leds[LED_GREEN], 0);

                // Set the LED duty cycle
                uint32_t duty = 0;
                if (state_current == State::RED_SIGNAL)
                {
                    duty = (uint32_t)(duty_f * duty_max[LED_RED]);
                    ledC->updateDuty(LED_RED, duty);
                }
                else if (state_current == State::WHITE_SIGNAL)
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
            if (elapsed_time > (float)AUDIO_BUFFER_SIZE / (float)SAMPLE_RATE * 1000.0f)
            {
                // elapsed_time must be less than AUDIO_BUFFER_SIZE/SAMPLE_RATE*1000(msec)
                printf(
                    "elapsed_time must be less than %f(msec)\n",
                    (float)AUDIO_BUFFER_SIZE / (float)SAMPLE_RATE * 1000.0f);
            }
        }
        break;

        case State::RED_CALIBRATION:
        case State::WHITE_CALIBRATION:
        {
            if (state_new != state_current)
            {
                state_current = state_new;

                // Turn off all LEDs
                for (int i = 0; i < leds.size(); i++)
                    ledC->updateDuty(leds[i], 0);

                counter = 0;
                timer->start();

                if (state_current == State::RED_CALIBRATION)
                    ledC->updateDuty(LED_RED, duty_max[LED_RED]);
                else if (state_current == State::WHITE_CALIBRATION)
                    ledC->updateDuty(LED_WHITE, duty_max[LED_WHITE]);
            }

            float elapsed_time = timer->measure();

            if (elapsed_time >= CALIB_PERIOD_SEC * 1000)
            {
                if (state_current == State::RED_CALIBRATION)
                    ledC->updateDuty(LED_RED, duty_max[LED_RED]);
                else if (state_current == State::WHITE_CALIBRATION)
                    ledC->updateDuty(LED_WHITE, duty_max[LED_WHITE]);

                timer->start();
            }
            else if (elapsed_time >= CALIB_PERIOD_SEC * 500)
            {
                if (state_current == State::RED_CALIBRATION)
                    ledC->updateDuty(LED_RED, 0);
                else if (state_current == State::WHITE_CALIBRATION)
                    ledC->updateDuty(LED_WHITE, 0);
            }

            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        break;

        case State::STATE_7:
            state_current = state_new;
            vTaskDelay(10 / portTICK_PERIOD_MS);
            break;
        }
    }
}
