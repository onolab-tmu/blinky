//
//  Created by Hisaharu SUZUKI
//  Copyright © 2019年 Hisaharu SUZUKI. All rights reserved.
//

#ifndef __EVX_PCM_SERVER__
#define __EVX_PCM_SERVER__

#include <vector>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "EVXAudioRecorder.h"

using namespace std;

#define CHANNEL_MODE_L (0)
#define CHANNEL_MODE_R (1)
#define CHANNEL_MODE_LR (2)

class EVXPCMServer
{    
public:
    EVXPCMServer(I2S_AUDIOCALLBACK callback, int audio_cpu_number, int queueLen);
    virtual ~EVXPCMServer();
    esp_err_t start();
    esp_err_t stop();
    static void send_pcm_task(void *arg);
    bool pushPCM(int16_t* pcm);
private:
    EVXAudioRecorder* recorder;
    I2S_AUDIOCALLBACK callback;
    void* param;
    int audio_cpu_number;

    xQueueHandle queue;
    int32_t channelmode;
    int32_t channels;
    int32_t samplerate;
    int sport;
    int timeout;
    bool is_running;
    bool finish;
    bool accepting;
    int get_socket_error_code(int sockfd);
    int show_socket_error_reason(const char *str, int sockfd);
    esp_err_t IRAM_ATTR run_tcp_server(void);
};

#endif
