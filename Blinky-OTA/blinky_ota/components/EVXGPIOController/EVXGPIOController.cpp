//
//  Created by Yaushiko NAGATOMO
//  Copyright © 2018年 Yasuhiko NAGATOMO. All rights reserved.
//

#include "EVXGPIOController.h"

EVXGPIOController::EVXGPIOController(vector<int> gpios)
{
    _gpios = gpios;
    
    uint64_t mask = 0;
    for (int i=0; i<_gpios.size(); i++) {
        mask |= (uint64_t)1<<_gpios[i];
    }
    
    gpio_config_t io_config;
    io_config.intr_type = GPIO_INTR_DISABLE;
    io_config.mode = GPIO_MODE_INPUT;
    io_config.pin_bit_mask = mask;
    io_config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_config.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_config);
    
}

int EVXGPIOController::getState()
{
    int state = 0;
    
    for (int i=0; i<_gpios.size(); i++) {
        if (gpio_get_level((gpio_num_t)_gpios[i]) == 1) {
            state |= 1<<i;
        }
    }
    
    return state;
}
