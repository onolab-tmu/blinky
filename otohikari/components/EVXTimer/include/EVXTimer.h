#ifndef __EVX_TIMER__
#define __EVX_TIMER__

#include "freertos/FreeRTOS.h"
#include "driver/timer.h"

// for measuring elapsed time.
class EVXTimer
{
private:
    double start_count, end_cout;
    
public:
    EVXTimer();
    
    void start();
    float measure(); // return elapsed time(msec).
};

#endif
