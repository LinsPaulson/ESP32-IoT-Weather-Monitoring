#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_config.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "dht11.h"
#include "mqtt_client.h"

static const char *TAG = "ESP32_THINGSBOARD";
#define DHT11_GPIO GPIO_NUM_4

static esp_mqtt_client_handle_t mqtt_client = NULL;

// ---------------- Wi-Fi Event Handler ----------------
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "Wi-Fi started, connecting...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "Wi-Fi disconnected. Retrying in 5 sec...");
                vTaskDelay(pdMS_TO_TICKS(5000));
                esp_wifi_connect();
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event_ip = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected to Wi-Fi, IP: " IPSTR, IP2STR(&event_ip->ip_info.ip));
    }
}

// Initialize Wi-Fi
void wifi_init(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

// ---------------- MQTT Event Handler ----------------
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            break;
        default:
            break;
    }
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://" THINGSBOARD_SERVER,
        .credentials.username = THINGSBOARD_TOKEN,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}


// ---------------- DHT Task ----------------
void dht_task(void *pvParameters) {
    dht11_data_t data;
    while (1) {
        int retry = 0;
        while (dht11_read(&data) != ESP_OK && retry < 3) {
            retry++;
            vTaskDelay(pdMS_TO_TICKS(2000)); // wait 1s before retry
        }

        if (retry == 3) {
            ESP_LOGW(TAG, "Failed to read DHT11 after 3 attempts");
        } else {
            char payload[64];
            snprintf(payload, sizeof(payload),
                     "{\"temperature\": %d, \"humidity\": %d}",
                     data.temperature, data.humidity);
            ESP_LOGI(TAG, "Publishing: %s", payload);
            esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", payload, 0, 1, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10000)); // every 10s
    }
}

// ---------------- Main ----------------
void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Wi-Fi debug logs
    esp_log_level_set("wifi", ESP_LOG_INFO);
    esp_log_level_set("wifi:sta", ESP_LOG_DEBUG);

    wifi_init();
    mqtt_app_start();
    dht11_init(DHT11_GPIO);

    xTaskCreate(dht_task, "dht_task", 4096, NULL, 5, NULL);
}
