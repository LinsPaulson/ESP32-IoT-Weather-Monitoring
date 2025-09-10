#ifndef DHT11_H
#define DHT11_H

#include "driver/gpio.h"
#include "esp_err.h"

typedef struct {
    int temperature; // Celsius
    int humidity;    // Percentage
} dht11_data_t;

// Initialize DHT11 sensor on given GPIO
esp_err_t dht11_init(gpio_num_t pin);

// Read data from DHT11 sensor
esp_err_t dht11_read(dht11_data_t *data);

#endif // DHT11_H
