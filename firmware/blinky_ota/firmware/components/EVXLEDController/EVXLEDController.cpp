//
//  Created by Yaushiko NAGATOMO
//  Copyright © 2017年 Yasuhiko NAGATOMO. All rights reserved.
//

#include "EVXLEDController.h"
#include <stdio.h>

EVXLEDController::EVXLEDController(ledc_timer_bit_t resolution, uint32_t frequency, vector<int>gpios)
{
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer;
    ledc_timer.duty_resolution = resolution;
    ledc_timer.freq_hz = frequency;
    ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_timer.timer_num = LEDC_TIMER_0;
    
    ledc_timer_config(&ledc_timer);
    
    // Configuration parameters of LEDC channels
    int gpios_count = gpios.size();
    if (gpios_count >= LEDC_CHANNEL_MAX) {
        printf("EVXLEDController / The number of GPIOs must be less than or equal to %d\n", LEDC_CHANNEL_MAX);
        gpios_count = LEDC_CHANNEL_MAX;
    }
    
    for (int i=0; i<gpios_count; i++) {
        ledc_channel_config_t led;
        led.channel = (ledc_channel_t)i;
        led.duty = 0;
        led.gpio_num = gpios[i];
        led.speed_mode = LEDC_HIGH_SPEED_MODE;
        led.timer_sel = LEDC_TIMER_0;
        
        ledc_channel_config(&led);
        leds[gpios[i]] = led;
    }
}

void EVXLEDController::updateDuty(int gpio, uint32_t duty)
{
    ledc_channel_config_t led = leds[gpio];
    ledc_set_duty(led.speed_mode, led.channel, duty);
    ledc_update_duty(led.speed_mode, led.channel);
}
