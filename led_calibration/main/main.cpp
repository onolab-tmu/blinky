#include <vector>
#include <math.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "EVXLEDController.h"

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
float camera_corr_lut[4] = {
  32.33597408,  // red
  9.02059563,   // white
  15.61660888,  // blue
  8.26709997,   // green
};

float camera_pre_correction(float d, int n)
{
  float b = camera_corr_lut[n];

  return (powf(b, d) - 1.) / (b - 1.);
}



// LED
#define LED_GREEN (25) // Left  Bottom
#define LED_BLUE (27)  // Left  Top
#define LED_RED (26)   // Right Bottom
#define LED_WHITE (15) // Right Top
#define LED_LEFT LED_RED
#define LED_RIGHT LED_GREEN
#define LED_RESOLUTION LEDC_TIMER_12_BIT
#define LED_FREQUENCY (16000)

// LED & AUDIO
#define MIN_DB (-70.0f)

#define TIMESTEP 60
#define STEP 1e-3

void main_process()
{
    vector<int> leds{LED_RED, LED_WHITE, LED_BLUE, LED_GREEN};
    
    // for controlling LEDs.
    EVXLEDController* ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);
    
    uint32_t duty_max = (uint32_t)(1 << LED_RESOLUTION);
    printf("LED Resolution: %d\n", (int)duty_max);
    uint32_t duty = 0;
    float duty_f;

    printf("test map_pwm %e %e\n", map_pwm(0.), map_pwm(1.));
    for (int n = 0 ; n < 4 ; n++)
      printf("test camera_corr %d %e %e\n", n, camera_pre_correction(0., n), camera_pre_correction(1., n));
    
    while (1) {

      // loop over all LEDs
      for (int n = 0 ; n < 4 ; n++)
      {

        // turn off all LEDs
        for (int m = 0 ; m < 4 ; m++)
          ledC->updateDuty(leds[m], 0);

        if (leds[n] == LED_RED)
        {

          float time = STEP;
          int counter = 0;

          // Now slowly ramp up one led
          while (time <= 1.)
          {
            /*
            float dB = (0-MIN_DB) * time;  
            duty_f = 1. - (dB / MIN_DB);
            */
            duty_f = time;  // ramp up the LED directly in the decibel domain

            if (duty_f < 0.)
              duty_f = 0.;
            if (duty_f > 1.)
              duty_f = 1.;

            duty_f = map_pwm(duty_f);
            duty_f = camera_pre_correction(duty_f, n);

            duty = (int)(duty_f * duty_max);

            ledC->updateDuty(leds[n], duty);

            vTaskDelay(TIMESTEP / portTICK_PERIOD_MS);

            time += STEP;
            counter += 1;
          }

        }
        else if (leds[n] == LED_BLUE)
        {
          // Use the Blue LED as reference at half PWM resolution
          ledC->updateDuty(leds[2], 1 << (LED_RESOLUTION - 2));
          vTaskDelay(10000 / portTICK_PERIOD_MS);  // 10 s
        }
        else
        {
          // This time, we will only work with RED and BLUE
          continue;
        }
      }
    }
}

void app_main(void)
{
    xTaskCreatePinnedToCore((TaskFunction_t)&main_process, "main_process", 16384, NULL, 0, NULL, 0);
}

