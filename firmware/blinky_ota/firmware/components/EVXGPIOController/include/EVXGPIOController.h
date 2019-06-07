//
//  Created by Yaushiko NAGATOMO
//  Copyright © 2018年 Yasuhiko NAGATOMO. All rights reserved.
//

#ifndef __EVX_GPIO_CONTROLLER__
#define __EVX_GPIO_CONTROLLER__

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include <vector>

using namespace std;

// for measuring elapsed time.
class EVXGPIOController
{
private:
    vector<int> _gpios;
    
public:
    EVXGPIOController(vector<int> gpios);
    
    int getState(); // return GPIO state.
};

#endif
