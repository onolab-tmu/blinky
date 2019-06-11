//
//  Created by Hisaharu SUZUKI
//  Copyright © 2019年 Hisaharu SUZUKI. All rights reserved.
//

#include <errno.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <vector>
#include <math.h>
#include "EVXTimer.h"
#include "EVXLEDController.h"
//#include "EVXAudioRecorder.h"
#include "EVXGPIOController.h"
#include "EVXWifiController.h"
#include "EVXPCMServer.h"


////
//// Begin parameters
////

//#define MODE_IPERF_TESTER
#ifdef MODE_IPERF_TESTER
    #include "iperf_tester.h"
#endif

// AUDIO
#define AUDIO_CPU_NUMBER (0)

// Threshold microphone LEVEL for LED illumination
#define MIN_DB (-45.0f)
#define MAX_DB (-10.0f)

// QUEUE Length of PCM buffer from server to client
// increasing this number may improve overflow
#define PCM_QUEUE_LEN (30)

// WifiParameter
// please define HOSTNAME/SSID/PASSWORD in patameter.h
// like
//#define HOSTNAME "rmic1"
//#define SSID "someones's iPhone"
//#define PASSWORD "password"
#include "wifiparameter.h"

////
//// End parameters
////

vector<int> leds{LED_BLUE, LED_GREEN, LED_WHITE, LED_RED};
vector<int> gpios{DIP_SWITCH_1, DIP_SWITCH_2, DIP_SWITCH_3}; // 2^3 = 8 pattern
vector< vector<int> > led_pattern{
        vector<int>{AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_NONE}, // GPIO switch : 0
        vector<int>{AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_RIGHT}, // GPIO switch : 1
        vector<int>{AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT}, // GPIO switch : 2
        vector<int>{AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_NONE}, // GPIO switch : 3
        vector<int>{AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_NONE, AUDIO_CHANNEL_LEFT}, // GPIO switch : 4
        vector<int>{AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT}, // GPIO switch : 5
        vector<int>{AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT, AUDIO_CHANNEL_LEFT}, // GPIO switch : 6
        vector<int>{AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT, AUDIO_CHANNEL_RIGHT} // GPIO switch : 7
    };

// for Brightness of LED
// +1 for turning off
uint32_t duties[AUDIO_CHANNELS+1];

// for measuring elapsed time.
//EVXTimer* timer;
// for controlling GPIOs.
EVXGPIOController* gpioC;
// for controlling LEDs.
EVXLEDController* ledC;
// for Wifi.
EVXWifiController* wifi;
// for PCM Server.
EVXPCMServer* server;
//

std::vector<int16_t> pcmbuffer((1+DMA_AUDIO_FRAMES*2)*PCM_QUEUE_LEN,0);
int currentPos=0;

uint32_t duty_max;

void audio_task(float* buffer, int8_t chs, int32_t frames, void* param)
 {
    //timer->start();
    float* audio_data = (float*)buffer;
    static uint16_t serial=0;
    int32_t* chmode=(int32_t*)param;
    
    int32_t SEND_AUDIO_CHANNEL_MODE=*chmode;
    int32_t SEND_AUDIO_CHANNELS=0;

    if(SEND_AUDIO_CHANNEL_MODE == CHANNEL_MODE_LR)
    {
        SEND_AUDIO_CHANNELS=2;
    }
    else
    {
        SEND_AUDIO_CHANNELS=1;
    }

    //ESP_LOGD("audio_task","SEND_AUDIO_CHANNELS=%d",SEND_AUDIO_CHANNELS);

    pcmbuffer[currentPos*(1+DMA_AUDIO_FRAMES*SEND_AUDIO_CHANNELS)]=serial;
    serial+=1;
    
    int16_t* _pcmbuffer =& pcmbuffer[1+currentPos*(1+DMA_AUDIO_FRAMES*SEND_AUDIO_CHANNELS)];
    
    if(SEND_AUDIO_CHANNEL_MODE == CHANNEL_MODE_LR)
    {
        for (int i=0; i<frames; i++) {
            for (int n=0; n<chs; n++) {
                //pcmbuffer[1+currentPos*(1+DMA_AUDIO_FRAMES*SEND_AUDIO_CHANNELS) + chs*i + n] = 32767*audio_data[chs*i + n];
                _pcmbuffer[chs*i + n] = 32767*audio_data[chs*i + n];
            }
        }
    }
    else if(SEND_AUDIO_CHANNEL_MODE == CHANNEL_MODE_L)
    {
        for (int i=0; i<frames; i++) {
            //pcmbuffer[1+currentPos*(1+DMA_AUDIO_FRAMES*SEND_AUDIO_CHANNELS) + i] = 32767*audio_data[chs*i + 0];
            _pcmbuffer[i] = 32767*audio_data[chs*i + 0];
        }
    }
    else if(SEND_AUDIO_CHANNEL_MODE == CHANNEL_MODE_R)
    {
        for (int i=0; i<frames; i++) {
            //pcmbuffer[1+currentPos*(1+DMA_AUDIO_FRAMES*SEND_AUDIO_CHANNELS) + i] = 32767*audio_data[chs*i + 1];
            _pcmbuffer[i] = 32767*audio_data[chs*i + 1];
        }
    }

    if(!server->pushPCM(&pcmbuffer[currentPos*(1+DMA_AUDIO_FRAMES*SEND_AUDIO_CHANNELS)]))
    {
        ESP_LOGE("audio_task","Error!! Buffer overflow!!");
    }
    currentPos+=1;
    currentPos%=PCM_QUEUE_LEN;
    
    for (int n=0; n<chs; n++) {
        float amp_val = 0.0f;
        float power_val = 0.0f;
        
        float maxV=0;

        for (int i=0; i<frames; i++) {
            if(maxV<fabs(audio_data[chs*i + n]))
            {
                maxV=fabs(audio_data[chs*i + n]);
            }

            amp_val += audio_data[chs*i + n];
            power_val += powf(audio_data[chs*i + n], 2.0f);
        }

        float power_excluding_offset = power_val/DMA_AUDIO_FRAMES - powf(amp_val/DMA_AUDIO_FRAMES, 2.0f);
        float dB = 10.0f * log10f(power_excluding_offset);
        
        float duty_f = (dB-MIN_DB)/(MAX_DB-MIN_DB);
        if (duty_f < 0.0f) duty_f = 0.0f;
        if (duty_f > 1.0f) duty_f = 1.0f;

        uint32_t duty = (uint32_t)(duty_f*duty_max);
        duties[n] = duty;
    }
        
    int gpio_state = gpioC->getState();
    for (int i=0; i<leds.size(); i++) {
        ledC->updateDuty(leds[i], duties[led_pattern[gpio_state][i]]);
    }
    
    /*float elapsed_time = timer->measure();
    if (elapsed_time > (float)DMA_AUDIO_FRAMES/(float)SAMPLE_RATE/1000.0f) {
        // elapsed_time must be less than AUDIO_BUFFER_SIZE/SAMPLE_RATE*1000(msec)
        printf("elapsed_time must be less than %f(msec)\n", (float)DMA_AUDIO_FRAMES/(float)SAMPLE_RATE*1000.0f);
    }*/
 }
//////////////////////////

void app_init()
{
    //esp_log_level_set("*", ESP_LOG_INFO);
#ifdef MODE_IPERF_TESTER
    iperf_tester_init();
#else

    wifi = new EVXWifiController();
    wifi->initialize(HOSTNAME);
    wifi->connectToAP(SSID,PASSWORD);

    server = new EVXPCMServer(audio_task,AUDIO_CPU_NUMBER,PCM_QUEUE_LEN);
    server->start();

#endif
    // Pattern of LED that changes according to GPIO state
    duties[AUDIO_CHANNEL_NONE] = 0;
        
    // for measuring elapsed time.
    //timer = new EVXTimer();
    // for controlling GPIOs.
    gpioC = new EVXGPIOController(gpios);
    // for controlling LEDs.
    ledC = new EVXLEDController(LED_RESOLUTION, LED_FREQUENCY, leds);
    // for recording.
    //recorder = new EVXAudioRecorder(audio_task,NULL,SAMPLE_RATE, DMA_AUDIO_FRAMES, I2S_BCLK, I2S_ADC_LRCLK, I2S_ADC_DATA, AUDIO_CPU_NUMBER);
    
    duty_max = (uint32_t)powf(2.0f, LED_RESOLUTION) - 1;
    
    // audio record start
    //recorder->start();
}

extern "C" void app_main(void)
{
    app_init();
#ifdef MODE_IPERF_TESTER
    iperf_tester_main();
#else
    while(1)
    {
        vTaskDelay(100);
    }

#endif
}