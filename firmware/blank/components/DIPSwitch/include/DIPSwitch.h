//
//  Created by Yaushiko NAGATOMO
//  Copyright © 2017年 Yasuhiko NAGATOMO. All rights reserved.
//

#ifndef __DIP_SWITCH__
#define __DIP_SWITCH__

#include <cstdint>
#include <driver/gpio.h>

class DIPSwitch
{
  private:
    uint8_t state = 0;
    gpio_num_t pin0, pin1, pin2;

  public:
    DIPSwitch(gpio_num_t _p0, gpio_num_t _p1, gpio_num_t _p2)
      : pin0(_p0), pin1(_p1), pin2(_p2)
    {
      gpio_config_t io_conf;

      //set as output mode
      io_conf.mode = GPIO_MODE_INPUT;

      //bit mask of the pins that you want to set,e.g.GPIO18/19
      io_conf.pin_bit_mask = (
          (1ULL<<pin0) | (1ULL<<pin1) | (1ULL<<pin2)
          );

      //disable pull-down mode
      io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;

      //disable pull-up mode
      io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

      //configure GPIO with the given settings
      gpio_config(&io_conf);

      // read initial value
      read();
    }

    uint8_t read()
    {
      /*
       * Read all the switches and return the state
       */
      uint8_t value = 0;
      value |= gpio_get_level(pin0);
      value |= gpio_get_level(pin1) << 1;
      value |= gpio_get_level(pin2) << 2;

      state = value;

      return state;
    }
};

#endif  // __DIP_SWITCH__
