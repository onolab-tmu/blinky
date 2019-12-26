/*
 * A class that abstract the DIP switch with 3 switches
 */
#ifndef __DIP_SWITCH_H__
#define __DIP_SWITCH_H__

#include <driver/gpio.h>

class DipSwitch3
{
  gpio_num_t pin1, pin2, pin3;

  public:
    DipSwitch3(gpio_num_t _pin1, gpio_num_t _pin2, gpio_num_t _pin3)
      : pin1(_pin1), pin2(_pin2), pin3(_pin3)
    {
      auto dip_switch_input_pin_sel = (
          (1ULL << pin1) | (1ULL << pin2) | (1ULL << pin3)
      );

      gpio_config_t io_conf;
      //set as output mode
      io_conf.mode = GPIO_MODE_INPUT;
      //bit mask of the pins that you want to set,e.g.GPIO18/19
      io_conf.pin_bit_mask = dip_switch_input_pin_sel;
      //disable pull-down mode
      io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
      //disable pull-up mode
      io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
      //configure GPIO with the given settings
      gpio_config(&io_conf);
    }

    char read()
    {
      char value = 0;
      value |= gpio_get_level(pin1);
      value |= gpio_get_level(pin2) << 1;
      value |= gpio_get_level(pin3) << 2;

      return value;
    }
};

#endif // __DIP_SWITCH_H__
