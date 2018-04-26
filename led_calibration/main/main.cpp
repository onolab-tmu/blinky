#include <vector>
#include <math.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "EVXLEDController.h"

extern "C" {
    void app_main(void);
}

// LED
#define LED_GREEN (25) // Left  Bottom
#define LED_BLUE (27)  // Left  Top
#define LED_RED (26)   // Right Bottom
#define LED_WHITE (15) // Right Top
#define LED_LEFT LED_RED
#define LED_RIGHT LED_GREEN
#define LED_RESOLUTION LEDC_TIMER_12_BIT
#define LED_FREQUENCY (16000)

// AUDIO
#define SAMPLE_RATE (16000)
#define AUDIO_BUFFER_SIZE (64)
#define I2S_BCK (23)
#define I2S_WS (14)
#define I2S_DATA_IN (22)
#define CPU_NUMBER (0)

// LED & AUDIO
#define MIN_DB (-70.0f)
#define MAX_DB (-10.0f)

void main_process()
{
    vector<int> leds{LED_RED, LED_WHITE, LED_BLUE, LED_GREEN};
    
    // for controlling LEDs.
    EVXLEDController* ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);
    
    uint32_t duty_max = (uint32_t)(1 << LED_RESOLUTION);
    printf("LED Resolution: %d\n", (int)duty_max);
    uint32_t duty = 0;
    
    while (1) {
        
        for (int n = 0 ; n < 4 ; n++)
            ledC->updateDuty(leds[n], duty);

        vTaskDelay(16 / portTICK_PERIOD_MS);

        duty++;
        if (duty == duty_max)
          duty = 0;

    }
}

void app_main(void)
{
    xTaskCreatePinnedToCore((TaskFunction_t)&main_process, "main_process", 16384, NULL, 0, NULL, 0);
}

