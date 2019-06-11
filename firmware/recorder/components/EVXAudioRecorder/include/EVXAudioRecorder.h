//
//  Created by Yaushiko NAGATOMO
//  Copyright © 2017年 Yasuhiko NAGATOMO. All rights reserved.
//

#ifndef __EVX_AUDIO_RECORDER__
#define __EVX_AUDIO_RECORDER__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include <vector>

using namespace std;

#define AUDIO_CHANNELS (2)
#define AUDIO_CHANNEL_LEFT (0)
#define AUDIO_CHANNEL_RIGHT (1)
#define AUDIO_CHANNEL_NONE (2)

//#define I2S_BCK (23)
#ifndef I2S_BCLK
#define I2S_BCLK (23)
#endif

//#define I2S_WS (14)
#ifndef I2S_ADC_LRCLK
#define I2S_ADC_LRCLK (14)
#endif

//#define I2S_DATA_IN (22)
#ifndef I2S_ADC_DATA
#define I2S_ADC_DATA (22)
#endif

#ifndef I2S_BITS_PER_SAMPLE
#define I2S_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_32BIT
#endif

#ifndef DMA_AUDIO_FRAMES
#define DMA_AUDIO_FRAMES (512)
#endif

#define DMA_BUFFER_SIZE (I2S_BITS_PER_SAMPLE/8*DMA_AUDIO_FRAMES*AUDIO_CHANNELS)

#ifndef DMA_BUFFER_COUNT
#define DMA_BUFFER_COUNT (2)
#endif

#define DMA_TOTAL_BUFFER_SIZE (DMA_BUFFER_SIZE*DMA_BUFFER_COUNT)

typedef void (* I2S_AUDIOCALLBACK)(float* buffer, int8_t chs, int32_t frames, void* param); 

struct task_parameter_t
{
    xQueueHandle queue;
    int cpu;
    int frame_size;
    int i2s_buffer_size;
    vector<char> i2s_buffer;
    vector<float> audio_buffer;
    I2S_AUDIOCALLBACK inputcallback;
    void* callbackparam;
};

// for recording.
class EVXAudioRecorder
{
private:
    bool now_recording;
    
    // task
    TaskHandle_t read_task;
    task_parameter_t task_parameter;

    i2s_config_t i2s_config;
    i2s_pin_config_t pin_config_rx;
    
public:
    // cpu_number: 0 or 1
    EVXAudioRecorder(I2S_AUDIOCALLBACK inputcallback, void* param, int sample_rate, int buffer_size, int bck, int ws, int data_in, int cpu_number);
    
    // start recording.
    void start();
    
    // stop recording.
    void stop();
    
    // Get the audio data.
    // This function waits until buffer is accumulated
    //float* wait_for_buffer_to_accumulate();
};

#endif
