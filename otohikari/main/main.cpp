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

// Options
#define ENABLE_MONITOR 0
#define ENABLE_LOG 0


// LED
#define LED_GREEN (25) // Left  Bottom
#define LED_BLUE (27)  // Left  Top
#define LED_RED (26)   // Right Bottom
#define LED_WHITE (15) // Right Top
#define LED_LEFT LED_RED
#define LED_RIGHT LED_GREEN
#define LED_RESOLUTION LEDC_TIMER_12_BIT
#define LED_FREQUENCY (16000)

// AUDIO
#define SAMPLE_RATE (16000)
#define AUDIO_BUFFER_SIZE (64)
#define I2S_BCK (23)
#define I2S_WS (14)
#define I2S_DATA_IN (22)
#define CPU_NUMBER (0)

// LED & AUDIO
#if ENABLE_LOG
#define MIN_DB (-70.0f)
#define MAX_DB (-10.0f)
#endif

#define MIN_LIN 1e-8
#define MAX_LIN 1e-3

void main_process()
{
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

#if ENABLE_MONITOR
    uint32_t counter = 0;
#endif
    
    while (1) {
        
        // Get the audio data.
        // This function waits until buffer is accumulated
        // audio_data is interleaved data. (length:AUDIO_BUFFER_SIZE x AUDIO_CHANNELS)
        float* audio_data = recorder->wait_for_buffer_to_accumulate();
        
        timer->start();
        for (int n=0; n<AUDIO_CHANNELS; n++) {
            double amp_val = 0.;
            double power_val = 0.;
            
            for (int i=0; i<AUDIO_BUFFER_SIZE; i++) {
                amp_val += audio_data[AUDIO_CHANNELS*i + n];
                power_val += audio_data[AUDIO_CHANNELS*i + n] * audio_data[AUDIO_CHANNELS*i + n];
            }

            amp_val /= AUDIO_BUFFER_SIZE;
            
            // This is the frame variance
            double power_excluding_offset = power_val/AUDIO_BUFFER_SIZE - amp_val * amp_val;

#if ENABLE_LOG
            // Log of variance
            float dB = 10.0f * log10f(power_excluding_offset);
            float duty_f = (dB-MIN_DB)/(MAX_DB-MIN_DB);
#else
            // Linear from variance to PWM
            float duty_f = (power_excluding_offset - MIN_LIN) / (MAX_LIN - MIN_LIN);
#endif

            if (duty_f < 0.0f) duty_f = 0.0f;
            if (duty_f > 1.0f) duty_f = 1.0f;

            uint32_t duty = (uint32_t)(duty_f*duty_max);
            ledC->updateDuty(leds[2*n], duty);

#if ENABLE_MONITOR
            if (counter % 50 == 0)
              printf("power_val=%e duty_f=%e duty=%d\n", (double)power_excluding_offset, (double)duty_f, (int)duty);
            counter += 1;
#endif
            
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

