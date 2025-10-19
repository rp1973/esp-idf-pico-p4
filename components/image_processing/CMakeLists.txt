idf_component_register(
    SRCS "image_processing.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_h264 camera_driver
)
