idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES dht11 mqtt
                       PRIV_REQUIRES nvs_flash esp_netif esp_event esp_wifi)