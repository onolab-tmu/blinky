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

#define LUT_SIZE 61
#define LUT_BASE
float lut[LUT_SIZE] = {
  0.,                    0.008773874102714685, 0.017523573381825086,
  0.02607612819282401,   0.034620136083068465, 0.04331733831488338,
  0.05230047386528325,   0.061675388827087674, 0.07152582470159385,
  0.08191960702011791,   0.09291522321689072,  0.10456801706542795,
  0.11693543765260839,   0.13008096516534118,  0.1440764950652642,
  0.15900309789588218,   0.174950185372309,    0.19201320590535764,
  0.21029006568024444,   0.22987652620980514,  0.2508608672761513,
  0.2733181267338959,    0.29730423713112,     0.32285037488391344,
  0.34995782217678406,   0.37859361622309096,  0.4086872263716097,
  0.44012845815276036,   0.4727667360874842,   0.5064118652974956,
  0.5408363170249617,    0.575779026456492,    0.610950634117771, 
  0.64604004592588,      0.680722133122291,    0.7146663431266431,
  0.747545947214615,     0.7790476121989043,   0.8088809523456144,
  0.8367876959550755,    0.8625500897419622,   0.8859981647299889,
  0.9070155011972668,    0.9255431586351102,   0.9415814810813828,
  0.9551895499249787,    0.9664821367163857,   0.9756241090261105,
  0.982822364333689,     0.9883155116706936,   0.9923616896472224,
  0.9952251039284353,    0.9971620885614387,   0.9984077451488259,
  0.9991644930891165,    0.9995941743218074,   0.9998156985914106,
  0.9999105915463267,    0.9999392193803281,   0.999970911572436,
  1.
};

float map_pwm(float frac)
{
  /*
   * the input should be normalized so that
   * the maximum value is 0 and the minimum value
   */

  int p;
  float p_f;
  float pwm_f;

  p_f = (frac * (LUT_SIZE - 1));
  p = (int)(p_f);

  pwm_f = (p_f - p) * (lut[p+1] - lut[p]) + lut[p];

  return pwm_f;

}

/* Camera correction */
float camera_corr_lut[8] = {
  37.82324192, 41.3588755,  // red
  8.71495763, 9.94289493, // white
  15.34113082, 16.60783334, // blue
  7.26587906, 8.26548027 // green
};

float camera_pre_correction(float d, int n)
{
  float a = camera_corr_lut[2*n];
  float b = camera_corr_lut[2*n+1];

  return (powf(b, d) - 1.) / a;
}

// Options
#define ENABLE_MONITOR 1
#define ENABLE_LOG 1


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
#define MIN_DB (-80.0f)
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
            float duty_f = (dB - MIN_DB) / (MAX_DB - MIN_DB);
#else
            // Linear from variance to PWM
            float duty_f = (power_excluding_offset - MIN_LIN) / (MAX_LIN - MIN_LIN);
#endif

            if (duty_f < 0.0f) duty_f = 0.0f;
            if (duty_f > 1.0f) duty_f = 1.0f;

#if ENABLE_LOG
            duty_f = map_pwm(duty_f);
            duty_f = camera_pre_correction(duty_f, 2*n);
#endif

            uint32_t duty = (uint32_t)(duty_f*duty_max);
            ledC->updateDuty(leds[2*n], duty);

#if ENABLE_MONITOR
            if (counter % 50 == 0)
              printf("power_val=%e db=%e duty_f=%e duty=%d\n", (double)power_excluding_offset, (double)dB, (double)duty_f, (int)duty);
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

