//
//  Created by Yaushiko NAGATOMO
//  Copyright © 2018年 Yasuhiko NAGATOMO. All rights reserved.
//

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota.cpp"

extern "C" {
    void app_main(void);
}

void app_main(void)
{
    xTaskCreatePinnedToCore((TaskFunction_t)&ota_task, "ota_task", 16384, NULL, 5, NULL, 0);
}

