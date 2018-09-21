//
//  Created by Yaushiko NAGATOMO
//  Copyright © 2018年 Yasuhiko NAGATOMO. All rights reserved.
//

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "otohikari.cpp"

#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASSWORD
#define SERVER_IP   CONFIG_SERVER_IP
#define SERVER_PORT CONFIG_SERVER_PORT
#define BIN_FILENAME "blinky.bin"
#define HASH_FILENAME "hash.txt"
#define CONSTRAINT_FILENAME "constraint.bin"
#define MY_NUMBER CONFIG_BLINKY_NUMBER
#define BUFFSIZE 1024
#define TEXT_BUFFSIZE 1024
#define STORAGE_NAMESPACE "storage"

static const char *TAG = "blinky";
/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };
/*an packet receive buffer*/
static char text[BUFFSIZE + 1] = { 0 };
/* an image total length*/
static int binary_file_length = 0;
/*socket id*/
static int socket_id = -1;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
 but we only care about one event - are we connected
 to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

static void __attribute__((noreturn)) switch_to_blinky_process()
{
    close(socket_id);
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    
    xTaskCreatePinnedToCore((TaskFunction_t)&main_process, "main_process", 16384, NULL, 0, NULL, 0);
    (void)vTaskDelete(NULL);
    
    while (1) {
        ;
    }
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
        case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
        switch_to_blinky_process();
        break;
        default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASS);
    wifi_config.sta.bssid_set = false;
    
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

/*read buffer by byte still delim ,return read bytes counts*/
static int read_until(char *buffer, char delim, int len)
{
    //  /*TODO: delim check,buffer check,further: do an buffer length limited*/
    int i = 0;
    while (buffer[i] != delim && i < len) {
        ++i;
    }
    return i + 1;
}

static int past_http_header_position(char text[], int total_len) {
    /* i means current position */
    int i = 0, i_read_len = 0;
    while (text[i] != 0 && i < total_len) {
        i_read_len = read_until(&text[i], '\n', total_len);
        // if we resolve \r\n line,we think packet header is finished
        if (i_read_len == 2) {
            return i+2;
        }
        i += i_read_len;
    }
    return 0;
}

/* resolve a packet from http socket
 * return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
 * otherwise return false
 * */
static bool read_past_http_header(char text[], int total_len, esp_ota_handle_t update_handle)
{
    int position = past_http_header_position(text, total_len);
    if (position != 0) {
        int i_write_len = total_len - position;
        memset(ota_write_data, 0, BUFFSIZE);
        /*copy first http packet body to write buffer*/
        memcpy(ota_write_data, &(text[position]), i_write_len);
        
        esp_err_t err = esp_ota_write( update_handle, (const void *)ota_write_data, i_write_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error: esp_ota_write failed (%s)!", esp_err_to_name(err));
            return false;
        } else {
            ESP_LOGI(TAG, "esp_ota_write header OK");
            binary_file_length += i_write_len;
        }
        return true;
    }
    return false;
}

static bool connect_to_http_server()
{
    ESP_LOGI(TAG, "Server IP: %s Server Port:%s", SERVER_IP, SERVER_PORT);
    
    int  http_connect_flag = -1;
    struct sockaddr_in sock_info;
    
    socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_id == -1) {
        ESP_LOGE(TAG, "Create socket failed!");
        return false;
    }
    
    // set connect info
    memset(&sock_info, 0, sizeof(struct sockaddr_in));
    sock_info.sin_family = AF_INET;
    sock_info.sin_addr.s_addr = inet_addr(SERVER_IP);
    sock_info.sin_port = htons(atoi(SERVER_PORT));
    
    // connect to http server
    http_connect_flag = connect(socket_id, (struct sockaddr *)&sock_info, sizeof(sock_info));
    if (http_connect_flag == -1) {
        ESP_LOGE(TAG, "Connect to server failed! errno=%d", errno);
        close(socket_id);
        return false;
    } else {
        ESP_LOGI(TAG, "Connected to server");
        return true;
    }
    return false;
}

static void ota_task(void *pvParameter)
{
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    
    initialise_wifi();
    
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;
    
    ESP_LOGI(TAG, "Starting OTA ...");
    
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    
    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);
    
    /* Wait for the callback to set the CONNECTED_BIT in the
     event group.
     */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connect to Wifi ! Start to Connect to Server....");
    
    /*send GET request to http server*/
    const char *GET_FORMAT =
    "GET %s HTTP/1.0\r\n"
    "Host: %s:%s\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n\r\n";
    
    // hash
    /*connect to http server*/
    if (connect_to_http_server()) {
        ESP_LOGI(TAG, "Connected to http server");
    } else {
        ESP_LOGE(TAG, "Connect to http server failed!");
        switch_to_blinky_process();
    }
    
    char *http_request = NULL;
    int get_len = asprintf(&http_request, GET_FORMAT, HASH_FILENAME, SERVER_IP, SERVER_PORT);
    if (get_len < 0) {
        ESP_LOGE(TAG, "Failed to allocate memory for GET request buffer");
        switch_to_blinky_process();
    }
    int res = send(socket_id, http_request, get_len, 0);
    free(http_request);
    
    if (res < 0) {
        ESP_LOGE(TAG, "Send GET request to server failed");
        switch_to_blinky_process();
    } else {
        ESP_LOGI(TAG, "Send GET request to server succeeded");
    }

    int remain = 32;
    char hash[32+1] = {0};
    bool resp_body_start = false, flag = true;
    /*deal with all receive packet*/
    while (flag) {
        memset(text, 0, TEXT_BUFFSIZE);
        int buff_len = recv(socket_id, text, TEXT_BUFFSIZE, 0);
        if (buff_len < 0) { /*receive error*/
            ESP_LOGE(TAG, "Error: receive data error! errno=%d", errno);
            close(socket_id);
            flag = false;
            //switch_to_blinky_process();
        } else if (buff_len > 0 && !resp_body_start) { /*deal with response header*/
            int pos = past_http_header_position(text, buff_len);
            if (pos > 0) {
                resp_body_start = true;
                
                int write_len = buff_len - pos;
                remain -= write_len;
                memcpy(hash, &text[pos], write_len);
            }
            
        } else if (buff_len > 0 && resp_body_start) { /*deal with response body*/
            if (remain > 0) {
                int write_len = remain;
                if (write_len > buff_len) {
                    write_len = buff_len;
                }
                
                remain -= write_len;
                memcpy(hash, text, write_len);
            }
        } else if (buff_len == 0) {  /*packet over*/
            flag = false;
            ESP_LOGI(TAG, "Connection closed, all packets received");
            close(socket_id);
        } else {
            ESP_LOGE(TAG, "Unexpected recv result");
        }
    }
    
    // compare
    bool isNewVersion = false;
    
    nvs_handle my_handle;
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS Open failed.");
        switch_to_blinky_process();
    }
    
    size_t required_size = 0;
    err = nvs_get_blob(my_handle, "blinky_hash", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "NVS get failed.");
        switch_to_blinky_process();
    }
    
    if (required_size == 0) {
        ESP_LOGE(TAG, "blinky_hash is not saved yet!");
        isNewVersion = true;
    } else {
        char* blinky_hash = (char*)malloc((required_size+1) * sizeof(char));
        err = nvs_get_blob(my_handle, "blinky_hash", blinky_hash, &required_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS get failed.");
            switch_to_blinky_process();
        }
        blinky_hash[required_size] = 0;
        
        if (strcmp(blinky_hash, hash) != 0) {
            isNewVersion = true;
        }
        
        free(blinky_hash);
    }
    
    if (isNewVersion) {
        // write
        size_t write_size = 32;
        err = nvs_set_blob(my_handle, "blinky_hash", hash, write_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS save failed.");
            switch_to_blinky_process();
        }
        
        err = nvs_commit(my_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "NVS commit failed.");
            switch_to_blinky_process();
        }
    }
    
    nvs_close(my_handle);
    
    if (!isNewVersion) {
        
        switch_to_blinky_process();
    }
    
    // Constraint
    int32_t myNumber = atoi(MY_NUMBER);
    bool isTargetDevice = false;
    
    /*connect to http server*/
    if (connect_to_http_server()) {
        ESP_LOGI(TAG, "Connected to http server");
    } else {
        ESP_LOGE(TAG, "Connect to http server failed!");
        switch_to_blinky_process();
    }
    
    http_request = NULL;
    get_len = asprintf(&http_request, GET_FORMAT, CONSTRAINT_FILENAME, SERVER_IP, SERVER_PORT);
    if (get_len < 0) {
        ESP_LOGE(TAG, "Failed to allocate memory for GET request buffer");
        switch_to_blinky_process();
    }
    res = send(socket_id, http_request, get_len, 0);
    free(http_request);
    
    if (res < 0) {
        ESP_LOGE(TAG, "Send GET request to server failed");
        switch_to_blinky_process();
    } else {
        ESP_LOGI(TAG, "Send GET request to server succeeded");
    }
    
    resp_body_start = false;
    flag = true;
    int temp_char_len = 0;
    /*deal with all receive packet*/
    while (flag) {
        memset(&text[temp_char_len], 0, TEXT_BUFFSIZE-temp_char_len);
        int buff_len = recv(socket_id, &text[temp_char_len], TEXT_BUFFSIZE-temp_char_len, 0);
        temp_char_len = 0;
        
        if (buff_len < 0) { /*receive error*/
            ESP_LOGE(TAG, "Error: receive data error! errno=%d", errno);
            close(socket_id);
            flag = false;
            //switch_to_blinky_process();
        } else if (buff_len > 0 && !resp_body_start) { /*deal with response header*/
            int pos = past_http_header_position(text, buff_len);
            if (pos > 0) {
                resp_body_start = true;
                
                int write_len = buff_len - pos;
                while (write_len >= 4) {
                    int32_t* number = (int32_t*)&text[pos];
                    if (myNumber == *number) {
                        isTargetDevice = true;
                    }
                    
                    pos += 4;
                    write_len -= 4;
                }
                
                if (write_len != 0) {
                    temp_char_len = write_len;
                    
                    for (int i=0; i<temp_char_len; i++) {
                        text[i] = text[pos+i];
                    }
                }
            }
            
        } else if (buff_len > 0 && resp_body_start) { /*deal with response body*/
            int pos = 0;
            int write_len = buff_len;
            while (write_len >= 4) {
                int32_t* number = (int32_t*)&text[pos];
                if (myNumber == *number) {
                    isTargetDevice = true;
                }
                
                pos += 4;
                write_len -= 4;
            }
            
            if (write_len != 0) {
                temp_char_len = write_len;
                
                for (int i=0; i<temp_char_len; i++) {
                    text[i] = text[pos+i];
                }
            }
        } else if (buff_len == 0) {  /*packet over*/
            flag = false;
            ESP_LOGI(TAG, "Connection closed, all packets received");
            close(socket_id);
        } else {
            ESP_LOGE(TAG, "Unexpected recv result");
        }
    }
    
    if (!isTargetDevice) {
        switch_to_blinky_process();
    }
    
    // app
    /*connect to http server*/
    if (connect_to_http_server()) {
        ESP_LOGI(TAG, "Connected to http server");
    } else {
        ESP_LOGE(TAG, "Connect to http server failed!");
        switch_to_blinky_process();
    }
    
    http_request = NULL;
    get_len = asprintf(&http_request, GET_FORMAT, BIN_FILENAME, SERVER_IP, SERVER_PORT);
    if (get_len < 0) {
        ESP_LOGE(TAG, "Failed to allocate memory for GET request buffer");
        switch_to_blinky_process();
    }
    res = send(socket_id, http_request, get_len, 0);
    free(http_request);
    
    if (res < 0) {
        ESP_LOGE(TAG, "Send GET request to server failed");
        switch_to_blinky_process();
    } else {
        ESP_LOGI(TAG, "Send GET request to server succeeded");
    }
    
    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);
    
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        switch_to_blinky_process();
    }
    ESP_LOGI(TAG, "esp_ota_begin succeeded");
    
    resp_body_start = false;
    flag = true;
    /*deal with all receive packet*/
    TaskHandle_t led_task;
    xTaskCreatePinnedToCore((TaskFunction_t)&download_process, "download_process", 4096, NULL, 6, &led_task, 0);
    
    while (flag) {
        memset(text, 0, TEXT_BUFFSIZE);
        memset(ota_write_data, 0, BUFFSIZE);
        int buff_len = recv(socket_id, text, TEXT_BUFFSIZE, 0);
        if (buff_len < 0) { /*receive error*/
            ESP_LOGE(TAG, "Error: receive data error! errno=%d", errno);
            (void)vTaskDelete(led_task);
            switch_to_blinky_process();
        } else if (buff_len > 0 && !resp_body_start) { /*deal with response header*/
            memcpy(ota_write_data, text, buff_len);
            resp_body_start = read_past_http_header(text, buff_len, update_handle);
        } else if (buff_len > 0 && resp_body_start) { /*deal with response body*/
            memcpy(ota_write_data, text, buff_len);
            err = esp_ota_write( update_handle, (const void *)ota_write_data, buff_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error: esp_ota_write failed (%s)!", esp_err_to_name(err));
                (void)vTaskDelete(led_task);
                switch_to_blinky_process();
            }
            binary_file_length += buff_len;
            ESP_LOGI(TAG, "Have written image length %d", binary_file_length);
        } else if (buff_len == 0) {  /*packet over*/
            flag = false;
            ESP_LOGI(TAG, "Connection closed, all packets received");
            close(socket_id);
        } else {
            ESP_LOGE(TAG, "Unexpected recv result");
        }
    }
    
    ESP_LOGI(TAG, "Total Write binary data length : %d", binary_file_length);
    (void)vTaskDelete(led_task);
    
    if (esp_ota_end(update_handle) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed!");
        switch_to_blinky_process();
    }
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        switch_to_blinky_process();
    }
    ESP_LOGI(TAG, "Prepare to restart system!");
    esp_restart();
    
    while (1) {
        
    }
    return ;
}

