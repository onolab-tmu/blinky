//
//  Created by Yaushiko NAGATOMO
//  Copyright © 2017年 Yasuhiko NAGATOMO. All rights reserved.
//

#ifndef __EVX_LED_CONTROLLER__
#define __EVX_LED_CONTROLLER__

#include "freertos/FreeRTOS.h"
#include "driver/ledc.h"
#include <vector>
#include <map>

using namespace std;

// LED pin number
#define LED_BLUE (27)
#define LED_GREEN (25)
#define LED_WHITE (15)
#define LED_RED (26)
#define LED_RESOLUTION LEDC_TIMER_12_BIT
#define LED_FREQUENCY (1000)

// for controlling LEDs.
class EVXLEDController
{
private:
    map<int, ledc_channel_config_t> leds;
    
public:
    EVXLEDController(ledc_timer_bit_t resolution, uint32_t frequency, const vector<int>& gpios);
    void updateDuty(int gpio, uint32_t duty);
};

#endif
