idf_component_register(
    SRCS "camera_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_camera esp_driver_h264 freertos esp_timer
)
