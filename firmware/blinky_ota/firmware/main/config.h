/*
 * This file collects the hardware related configurations
 */
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <vector>
#include <map>
#include <driver/gpio.h>

// Declare the GPIO in an ENUM so that
// we can use friendly names
const int LED_RED(26);
const int LED_WHITE(15);
const int LED_BLUE(27);
const int LED_GREEN(25);
const int LED_LEFT = LED_RED;
const int LED_RIGHT = LED_GREEN;
const auto LED_RESOLUTION = LEDC_TIMER_12_BIT;
const int LED_FREQUENCY(16000);

map<int, uint32_t> duty_max {
  {LED_RED, 1200},
  {LED_WHITE, 1200},
  {LED_BLUE, 1000},
  {LED_GREEN, 4095}
};

// DIP SWITCHES
const auto DIP_SWITCH_1 = GPIO_NUM_13;
const auto DIP_SWITCH_2 = GPIO_NUM_32;
const auto DIP_SWITCH_3 = GPIO_NUM_33;

// AUDIO
const int SAMPLE_RATE (16000);
const size_t AUDIO_BUFFER_SIZE (64);
const int I2S_BCK (23);
const int I2S_WS (14);
const int I2S_DATA_IN (22);
const int CPU_NUMBER (0);

// DC removal filter coefficient
const float DC_REMOVAL_ALPHA(0.99f);

// Length of ramp used for calibration
const float CALIB_PERIOD_SEC(3.f);

// Clipping bound for power
const float MIN_DB(-80.0f);
const float MAX_DB(-10.0f);

// Options
const bool ENABLE_MONITOR (true);

#endif  // __CONFIG_H__
