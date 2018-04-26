#ifndef __EVX_LED_CONTROLLER__
#define __EVX_LED_CONTROLLER__

#include "freertos/FreeRTOS.h"
#include "driver/ledc.h"
#include <vector>
#include <map>

using namespace std;

// for controlling LEDs.
class EVXLEDController
{
private:
    map<int, ledc_channel_config_t> leds;
    
public:
    EVXLEDController(ledc_timer_bit_t resolution, uint32_t frequency, vector<int> gpios);
    void updateDuty(int gpio, uint32_t duty);
};

#endif
