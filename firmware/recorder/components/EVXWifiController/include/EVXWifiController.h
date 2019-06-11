//
//  Created by Yaushiko NAGATOMO
//  Copyright © 2018年 Yasuhiko NAGATOMO. All rights reserved.
//

#ifndef __EVX_WIFI_CONTROLLER__
#define __EVX_WIFI_CONTROLLER__

#include <vector>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"


using namespace std;

// for measuring elapsed time.
class EVXWifiController
{    
public:
    EVXWifiController();
    int initialize(const char* hostname);
    int connectToAP(const char* ssid, const char* password);
public:
    static bool auto_reconnect;
    static esp_err_t event_handler(void *ctx, system_event_t *event);
};

#endif
