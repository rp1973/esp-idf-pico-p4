idf_component_register(
    SRCS "connectivity.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_netif esp_event esp_wifi esp_eth lwip image_processing
)
