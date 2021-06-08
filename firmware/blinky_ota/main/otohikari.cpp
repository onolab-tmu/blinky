#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "EVXTimer.h"
#include "EVXLEDController.h"
#include "EVXAudioRecorder.h"
#include "EVXGPIOController.h"

#include "tiny_dnn/tiny_dnn.h"

// LED
#define LED_BLUE (27)
#define LED_GREEN (25)
#define LED_WHITE (15)
#define LED_RED (26)
#define LED_RESOLUTION LEDC_TIMER_12_BIT
#define LED_FREQUENCY (1000)

// GPIO
#define DIP_SWITCH_1 (13)
#define DIP_SWITCH_2 (32)
#define DIP_SWITCH_3 (33)

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

static void wifi_connected()
{
    vector<int> leds{LED_RED};
    EVXLEDController* ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);
    
    uint32_t duty_max = (uint32_t)powf(2.0f, LED_RESOLUTION) - 1;
    ledC->updateDuty(leds[0], duty_max);
    
    vTaskDelete(NULL);
}

static void server_connected()
{
    vector<int> leds{LED_RED, LED_GREEN};
    EVXLEDController* ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);
    
    uint32_t duty_max = (uint32_t)powf(2.0f, LED_RESOLUTION) - 1;
    ledC->updateDuty(leds[0], duty_max);
    ledC->updateDuty(leds[1], duty_max);
    
    vTaskDelete(NULL);
}

// for LED animation while downloading
static void download_process()
{
    vector<int> leds{LED_BLUE, LED_WHITE, LED_RED, LED_GREEN};
    EVXLEDController* ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);
    
    uint32_t duty_max = (uint32_t)powf(2.0f, LED_RESOLUTION) - 1;
    
    uint8_t rotate = 0;
    while (1) {
        for (int i=0; i<leds.size(); i++) {
            if (i == rotate) {
                ledC->updateDuty(leds[i], duty_max);
            } else {
                ledC->updateDuty(leds[i], 0);
            }
        }
        rotate = (rotate+1)%leds.size();
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// for main process
static void main_process()
{
    vector<int> leds{LED_BLUE, LED_GREEN, LED_WHITE, LED_RED};
    vector<int> gpios{DIP_SWITCH_1, DIP_SWITCH_2, DIP_SWITCH_3}; // 2^3 = 8 pattern

    // for Brightness of LED
    // +1 for turning off
    uint32_t duties[AUDIO_CHANNELS+1];
    duties[AUDIO_CHANNEL_NONE] = 0;
    // Pattern of LED that changes according to GPIO state
    vector< vector<int> > led_pattern{
        vector<int>{AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_NONE}, // GPIO switch : 0
        vector<int>{AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_RIGHT}, // GPIO switch : 1
        vector<int>{AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT}, // GPIO switch : 2
        vector<int>{AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_NONE}, // GPIO switch : 3
        vector<int>{AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_LEFT}, // GPIO switch : 4
        vector<int>{AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT}, // GPIO switch : 5
        vector<int>{AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT}, // GPIO switch : 6
        vector<int>{AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT} // GPIO switch : 7
    };

    // for measuring elapsed time.
    EVXTimer* timer = new EVXTimer();
    // for controlling GPIOs.
    EVXGPIOController* gpioC = new EVXGPIOController(gpios);
    // for controlling LEDs.
    EVXLEDController* ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);
    // for recording.
    EVXAudioRecorder* recorder = new EVXAudioRecorder(SAMPLE_RATE, AUDIO_BUFFER_SIZE, I2S_BCK, I2S_WS, I2S_DATA_IN, CPU_NUMBER);
    
    uint32_t duty_max = (uint32_t)powf(2.0f, LED_RESOLUTION) - 1;
    
    // audio record start
    recorder->start();

    while (1) {
        
        // Get the audio data.
        // This function waits until buffer is accumulated
        // audio_data is interleaved data. (length:AUDIO_BUFFER_SIZE x AUDIO_CHANNELS)
        float* audio_data = recorder->wait_for_buffer_to_accumulate();
        
        timer->start();
        for (int n=0; n<AUDIO_CHANNELS; n++) {
            float amp_val = 0.0f;
            float power_val = 0.0f;
            
            for (int i=0; i<AUDIO_BUFFER_SIZE; i++) {
                amp_val += audio_data[AUDIO_CHANNELS*i + n];
                power_val += powf(audio_data[AUDIO_CHANNELS*i + n], 2.0f);
            }
            
            float power_excluding_offset = power_val/AUDIO_BUFFER_SIZE - powf(amp_val/AUDIO_BUFFER_SIZE, 2.0f);
            float dB = 10.0f * log10f(power_excluding_offset);
            
            float duty_f = (dB-MIN_DB)/(MAX_DB-MIN_DB);
            if (duty_f < 0.0f) duty_f = 0.0f;
            if (duty_f > 1.0f) duty_f = 1.0f;
            
            uint32_t duty = (uint32_t)(duty_f*duty_max);
            duties[n] = duty;
        }
        
        int gpio_state = gpioC->getState();
        for (int i=0; i<leds.size(); i++) {
            ledC->updateDuty(leds[i], duties[led_pattern[gpio_state][i]]);
        }
        
        float elapsed_time = timer->measure();
        if (elapsed_time > (float)AUDIO_BUFFER_SIZE/(float)SAMPLE_RATE*1000.0f) {
            // elapsed_time must be less than AUDIO_BUFFER_SIZE/SAMPLE_RATE*1000(msec)
            printf("elapsed_time must be less than %f(msec)\n", (float)AUDIO_BUFFER_SIZE/(float)SAMPLE_RATE*1000.0f);
        }
    }
}
