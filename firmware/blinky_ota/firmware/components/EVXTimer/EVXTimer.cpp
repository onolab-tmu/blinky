//
//  Created by Yaushiko NAGATOMO
//  Copyright © 2017年 Yasuhiko NAGATOMO. All rights reserved.
//

#include "EVXTimer.h"

EVXTimer::EVXTimer()
{
    timer_config_t timer_config;
    timer_config.alarm_en = false;
    timer_config.counter_en = true;
    timer_config.counter_dir = TIMER_COUNT_UP;
    timer_config.divider = 80;
    
    timer_init(TIMER_GROUP_0, TIMER_0, &timer_config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
    timer_start(TIMER_GROUP_0, TIMER_0);
}

void EVXTimer::start()
{
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0); // reset
    timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &start_count);
}

float EVXTimer::measure()
{
    timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &end_cout);
    return (float)(end_cout - start_count) * 1000.0f;
}
