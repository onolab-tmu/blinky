#include <vector>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "EVXTimer.h"
#include "EVXLEDController.h"
#include "EVXAudioRecorder.h"

extern "C" {
    void app_main(void);
}

// LED
#define LED_GREEN (25) // Left  Bottom
#define LED_BLUE (27)  // Left  Top
#define LED_RED (26)   // Right Bottom
#define LED_WHITE (15) // Right Top
#define LED_LEFT LED_RED
#define LED_RIGHT LED_GREEN
#define LED_RESOLUTION LEDC_TIMER_10_BIT
#define LED_FREQUENCY (16000)

// AUDIO
#define SAMPLE_RATE (16000)
#define AUDIO_BUFFER_SIZE (64)
#define I2S_BCK (23)
#define I2S_WS (14)
#define I2S_DATA_IN (22)
#define CPU_NUMBER (0)

// LED & AUDIO
#define MIN_DB (-70.0f)
#define MAX_DB (-10.0f)

void main_process()
{
    vector<int> leds{LED_RED, LED_WHITE, LED_BLUE, LED_GREEN};
    
    // for measuring elapsed time.
    EVXTimer* timer = new EVXTimer();
    // for controlling LEDs.
    EVXLEDController* ledC = new EVXLEDController(LEDC_TIMER_12_BIT, LED_FREQUENCY, leds);
    // for recording.
    EVXAudioRecorder* recorder = new EVXAudioRecorder(SAMPLE_RATE, AUDIO_BUFFER_SIZE, I2S_BCK, I2S_WS, I2S_DATA_IN, CPU_NUMBER);
    
    uint32_t duty_max = (uint32_t)powf(2.0f, LED_RESOLUTION);
    
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
            //uint32_t neg_duty = (uint32_t)((1. - duty_f)*duty_max);
            ledC->updateDuty(leds[2*n], duty);
        }
        float elapsed_time = timer->measure();
        if (elapsed_time > (float)AUDIO_BUFFER_SIZE/(float)SAMPLE_RATE*1000.0f) {
            // elapsed_time must be less than AUDIO_BUFFER_SIZE/SAMPLE_RATE*1000(msec)
            printf("elapsed_time must be less than %f(msec)\n", (float)AUDIO_BUFFER_SIZE/(float)SAMPLE_RATE*1000.0f);
        }
    }
}

void app_main(void)
{
    xTaskCreatePinnedToCore((TaskFunction_t)&main_process, "main_process", 16384, NULL, 0, NULL, 0);
}

