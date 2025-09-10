#include "dht11.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h" // for esp_rom_delay_us()

static const char *TAG = "DHT11";
static gpio_num_t dht11_gpio;

// Initialize DHT11
esp_err_t dht11_init(gpio_num_t pin) {
    dht11_gpio = pin;
    gpio_reset_pin(dht11_gpio);
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11_gpio, 1); // idle high
    return ESP_OK;
}

// Wait for pin level with timeout
static int wait_for_level(int level, int timeout_us) {
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(dht11_gpio) != level) {
        if ((esp_timer_get_time() - start) > timeout_us) {
            return -1; // timeout
        }
    }
    return 0;
}

// Read DHT11 with retry logic
esp_err_t dht11_read(dht11_data_t *data) {
    if (data == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t bits[5] = {0};
    uint8_t byte = 0;

    // Start signal
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11_gpio, 0);
    esp_rom_delay_us(18000); // 18ms
    gpio_set_level(dht11_gpio, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(dht11_gpio, GPIO_MODE_INPUT);

    // Response
    if (wait_for_level(0, 80) < 0) return ESP_FAIL;
    if (wait_for_level(1, 80) < 0) return ESP_FAIL;

    // Read 40 bits
    for (int i = 0; i < 40; i++) {
        if (wait_for_level(0, 80) < 0) return ESP_FAIL;
        if (wait_for_level(1, 80) < 0) return ESP_FAIL;

        int64_t start = esp_timer_get_time();
        if (wait_for_level(0, 100) < 0) return ESP_FAIL;
        int64_t duration = esp_timer_get_time() - start;

        byte = (byte << 1) | (duration > 40);
        if ((i + 1) % 8 == 0) {
            bits[i / 8] = byte;
            byte = 0;
        }
    }

    // Checksum
    if ((uint8_t)(bits[0] + bits[1] + bits[2] + bits[3]) != bits[4]) {
        ESP_LOGW(TAG, "Checksum failed");
        return ESP_FAIL;
    }

    data->humidity = bits[0];
    data->temperature = bits[2];

    ESP_LOGI(TAG, "Temperature: %d°C, Humidity: %d%%", data->temperature, data->humidity);
    return ESP_OK;
}
