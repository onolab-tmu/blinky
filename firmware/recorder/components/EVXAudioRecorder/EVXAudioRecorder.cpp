//
//  Created by Yaushiko NAGATOMO
//  Copyright © 2017年 Yasuhiko NAGATOMO. All rights reserved.
//

#include "EVXAudioRecorder.h"
#include <stdio.h>

void audio_read_task(void* pvParameters)
{
    task_parameter_t* task_parameter = (task_parameter_t*)pvParameters;
    
    float max_24bit = (float)(1<<23);
    
    while (1) {
        uint32_t bytes_read = i2s_read_bytes(I2S_NUM_0, &task_parameter->i2s_buffer[0], task_parameter->i2s_buffer_size, portMAX_DELAY);
        int32_t *buffer_input_i32 = (int32_t *)&task_parameter->i2s_buffer[0];
        uint32_t samples_read = bytes_read / task_parameter->frame_size / AUDIO_CHANNELS;
        
        float signal;
        
        for(int ch=0;ch<AUDIO_CHANNELS;ch++){
            for (int i=0; i<samples_read; i++) {
                signal = (buffer_input_i32[AUDIO_CHANNELS*i+ch] >> 8)/max_24bit;
                task_parameter->audio_buffer[AUDIO_CHANNELS*i+ch] = signal;
            }
        }
        task_parameter->inputcallback(&task_parameter->audio_buffer[0],AUDIO_CHANNELS,samples_read,task_parameter->callbackparam);
    }
}

EVXAudioRecorder::EVXAudioRecorder(I2S_AUDIOCALLBACK inputcallback, void* param, int sample_rate, int buffer_size, int bck, int ws, int data_in, int cpu_number)
{
    // I2S setting
    //i2s_config_t i2s_config;
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    i2s_config.sample_rate = sample_rate;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE;
    i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = DMA_BUFFER_COUNT;
    i2s_config.dma_buf_len = DMA_AUDIO_FRAMES;
    i2s_config.use_apll = 1; // accurate clock
    i2s_config.fixed_mclk = 12288000;
    
    //i2s_pin_config_t pin_config_rx;
    pin_config_rx.bck_io_num = bck;
    pin_config_rx.ws_io_num = ws;
    pin_config_rx.data_out_num = I2S_PIN_NO_CHANGE;
    pin_config_rx.data_in_num = data_in;
    
    //i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

    //i2s_set_pin(I2S_NUM_0, &pin_config_rx);

    //i2s_stop(I2S_NUM_0);

    // task setting
    task_parameter.queue = xQueueCreate(10, sizeof(uint32_t));
    task_parameter.cpu = cpu_number;
    // audio setting
    task_parameter.frame_size = I2S_BITS_PER_SAMPLE / 8;
    task_parameter.i2s_buffer_size = DMA_BUFFER_SIZE;// DMA_TOTAL_BUFFER_SIZE;//64 * task_parameter.frame_size; // 64 samples
    task_parameter.i2s_buffer.resize(task_parameter.i2s_buffer_size);
    task_parameter.audio_buffer.resize(AUDIO_CHANNELS * DMA_AUDIO_FRAMES);
    task_parameter.inputcallback = inputcallback;
    task_parameter.callbackparam = param;
    
    now_recording = false;
    
}

void EVXAudioRecorder::start()
{
    if (now_recording) return;
    now_recording = true;
    
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config_rx);
    i2s_stop(I2S_NUM_0);

    i2s_start(I2S_NUM_0);
    
    xTaskCreatePinnedToCore((TaskFunction_t)&audio_read_task, "audio_read_task", 8192, &task_parameter, 5, &read_task, task_parameter.cpu);
}

void EVXAudioRecorder::stop()
{
    if (!now_recording) return;
    now_recording = false;
    
    i2s_stop(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_0);
    
    vTaskDelete(read_task);
}