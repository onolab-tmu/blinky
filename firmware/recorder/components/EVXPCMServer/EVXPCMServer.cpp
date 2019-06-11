//
//  Created by Hisaharu SUZUKI
//  Copyright © 2019年 Hisaharu SUZUKI. All rights reserved.
//

#include "EVXPCMServer.h"
#include "EVXAudioRecorder.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "EVXPCMServer"
#define TASK_PRIORITY 10
#define TASK_STACK 4096

EVXPCMServer::EVXPCMServer(I2S_AUDIOCALLBACK _callback, int _audio_cpu_number, int queueLen)
{
    sport=8080;
    is_running=false;
    finish=true;
    timeout=10;//10秒でタイムアウト
    accepting=false;
    
    queue = xQueueCreate(queueLen, sizeof(int16_t*));

    recorder=nullptr;
    callback=_callback;
    param=&channelmode;
    audio_cpu_number=_audio_cpu_number;
}

EVXPCMServer::~EVXPCMServer()
{
    
}

bool EVXPCMServer::pushPCM(int16_t* pcm)
{
    if(accepting){
        //printf("send=%p\n",pcm);
        portBASE_TYPE ret = xQueueSend(queue,&pcm,0);
        if(ret != pdTRUE)
        {
            ESP_LOGW(TAG,"pcm queue failed, queue len=%d",uxQueueMessagesWaiting(queue));
            return false;
        }
    }
    return true;
}

int EVXPCMServer::get_socket_error_code(int sockfd)
{
    uint32_t optlen = sizeof(int);
    int result;
    int err;

    /* get the error state, and clear it */
    err = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &result, &optlen);
    if (err == -1) {
        ESP_LOGE(TAG, "getsockopt failed: ret=%d", err);
        return -1;
    }

    return result;
}

int EVXPCMServer::show_socket_error_reason(const char *str, int sockfd)
{
    int err = get_socket_error_code(sockfd);

    if (err != 0) {
        ESP_LOGW(TAG, "%s error, error code: %d, reason: %s", str, err, strerror(err));
    }

    return err;
}

esp_err_t IRAM_ATTR EVXPCMServer::run_tcp_server(void)
{
    socklen_t addr_len = sizeof(struct sockaddr);
    struct sockaddr_in remote_addr;
    struct sockaddr_in addr;

    int listen_socket;
    struct timeval t;
    int sockfd;
    int opt;

    int _channels=2;

    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket < 0) {
        show_socket_error_reason("tcp server create", listen_socket);
        return ESP_FAIL;
    }

    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(sport);

    //get local ip
    tcpip_adapter_if_t ifx = TCPIP_ADAPTER_IF_STA;
    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_get_ip_info(ifx, &ip_info);

    addr.sin_addr.s_addr = ip_info.ip.addr;//set local ip
    if (bind(listen_socket, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        show_socket_error_reason("tcp server bind", listen_socket);
        close(listen_socket);
        return ESP_FAIL;
    }

    if (listen(listen_socket, 5) < 0) {
        show_socket_error_reason("tcp server listen", listen_socket);
        close(listen_socket);
        return ESP_FAIL;
    }

    while (!finish) {

        sockfd = accept(listen_socket, (struct sockaddr *)&remote_addr, &addr_len);
        if (sockfd < 0) {
            show_socket_error_reason("tcp server listen", listen_socket);
            close(listen_socket);
            return ESP_FAIL;
        } else {
            printf("accept: %s,%d\n", inet_ntoa(remote_addr.sin_addr), htons(remote_addr.sin_port));

            t.tv_sec = timeout;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
        }

        if(recv(sockfd, &channelmode, sizeof(channelmode), 0)!=sizeof(channelmode))
        {
            show_socket_error_reason("chmode recv", sockfd);
        }

        if(recv(sockfd, &samplerate, sizeof(samplerate), 0)!=sizeof(samplerate))
        {
            show_socket_error_reason("chmode recv", sockfd);
        }

        ESP_LOGW(TAG,"chmode=%d, samplerate=%d",channelmode,samplerate);

        if(channelmode==CHANNEL_MODE_L || channelmode==CHANNEL_MODE_R)
        {
            _channels=1;
            channels=1;
        }
        else if(channelmode==CHANNEL_MODE_LR)
        {
            _channels=2;
            channels=2;
        }

        if(recorder!=nullptr)
        {
            delete recorder;
            recorder=nullptr;
        }

        recorder = new EVXAudioRecorder(callback,param,samplerate, DMA_AUDIO_FRAMES, I2S_BCLK, I2S_ADC_LRCLK, I2S_ADC_DATA, audio_cpu_number);
        recorder->start();
        accepting=true;
        bool connected=true;
        while (!finish && connected) {
            int16_t* pcm=nullptr;
            portBASE_TYPE ret = xQueueReceive(queue,&pcm,0);
            int len = uxQueueMessagesWaiting(queue);
            if(len>10)
                ESP_LOGW(TAG,"send queue len=%d",len);
            if(ret==pdTRUE)
            {
                //printf("serial=%d\n", pcm[0]);
                int length=(1+DMA_AUDIO_FRAMES*_channels)*sizeof(int16_t);
                //printf("length=%d\n", length);
                while(length>0)
                {
                    int sent = send(sockfd, pcm, length, MSG_DONTWAIT);
                    if(sent>=0)
                    {
                        length-=sent;
                    }
                    else
                    {
                        int err=show_socket_error_reason("tcp server send", sockfd);
                        if(err==104){//Connection reset by peer
                            connected=false;
                            break;
                        }
                    }
                }
            }
        }
        accepting=false;
        close(sockfd);
        printf("closed: %s\n",inet_ntoa(remote_addr.sin_addr));
        recorder->stop();
    }

    finish = true;
    close(listen_socket);
    return ESP_OK;
}

void EVXPCMServer::send_pcm_task(void *arg)
{
    EVXPCMServer* _this=(EVXPCMServer*)arg;
    _this->run_tcp_server();
    _this->is_running = false;
    vTaskDelete(NULL);
}

esp_err_t EVXPCMServer::start()
{
    if (is_running) {
        ESP_LOGW(TAG, "pcm server is running");
        return ESP_FAIL;
    }

    finish=false;
    
    BaseType_t ret;
    ret = xTaskCreatePinnedToCore(EVXPCMServer::send_pcm_task, "send_pcm_task", TASK_STACK, this, TASK_PRIORITY, NULL, 1);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "create task %s failed", "send_pcm_task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t EVXPCMServer::stop(void)
{
    if (is_running) {
        finish = true;
    }

    while (is_running) {
        ESP_LOGI(TAG, "wait current pcm server to stop ...");
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }

    return ESP_OK;
}
