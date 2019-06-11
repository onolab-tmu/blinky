//
//  Created by Yaushiko NAGATOMO
//  Copyright © 2018年 Yasuhiko NAGATOMO. All rights reserved.
//

#include "EVXWifiController.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "tcpip_adapter.h"
#include "esp_event_loop.h"
#include "mdns.h"

#define TAG "EVXWifiController"
/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group=nullptr;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int IP4_CONNECTED_BIT = BIT0;
const int IP6_CONNECTED_BIT = BIT1;

bool EVXWifiController::auto_reconnect=true;

esp_err_t EVXWifiController::event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "wifi connected");
        /* enable ipv6 */
        tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, IP4_CONNECTED_BIT);
        ESP_LOGI(TAG, "got addr(ipv4)");
        break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
        xEventGroupSetBits(wifi_event_group, IP6_CONNECTED_BIT);
        ESP_LOGI(TAG, "got addr(ipv6)");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        if (EVXWifiController::auto_reconnect) {
            esp_wifi_connect();
        }
        xEventGroupClearBits(wifi_event_group, IP4_CONNECTED_BIT | IP6_CONNECTED_BIT);
        break;
    default:
        break;
    }
    mdns_handle_system_event(ctx, event);
    return ESP_OK;
}

EVXWifiController::EVXWifiController()
{

}

int EVXWifiController::initialize(const char* hostname)
{
    ESP_ERROR_CHECK( nvs_flash_init() );

    //initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", hostname);
    
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(EVXWifiController::event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    return 0;
}

int EVXWifiController::connectToAP(const char* ssid, const char* password)
{
    wifi_config_t wifi_config={};
    strncpy((char*) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if(password)
        strncpy((char*) wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    xEventGroupWaitBits(wifi_event_group, IP4_CONNECTED_BIT | IP6_CONNECTED_BIT,
                     false, true, portMAX_DELAY);

    return 0;
}
