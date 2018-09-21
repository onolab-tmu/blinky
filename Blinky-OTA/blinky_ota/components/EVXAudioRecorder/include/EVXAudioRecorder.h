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

struct task_parameter_t
{
    xQueueHandle queue;
    int cpu;
    int frame_size;
    int i2s_buffer_size;
    vector<char> i2s_buffer;
    int audio_buffer_size;
    vector<float> audio_buffer;
};

// for recording.
class EVXAudioRecorder
{
private:
    bool now_recording;
    
    // task
    TaskHandle_t read_task;
    task_parameter_t task_parameter;
    
public:
    // cpu_number: 0 or 1
    EVXAudioRecorder(int sample_rate, int buffer_size, int bck, int ws, int data_in, int cpu_number);
    
    // start recording.
    void start();
    
    // stop recording.
    void stop();
    
    // Get the audio data.
    // This function waits until buffer is accumulated
    float* wait_for_buffer_to_accumulate();
};

#endif
