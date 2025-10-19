#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"

#include "camera_driver.h"
#include "piccture.h"
#include "com.h"

typedef struct {
    encoder_handle_t encoder;
    transport_handle_t transport;
} camera_pipeline_t;

camera_config_t camera_default_config(void);
esp_err_t camera_driver_setup(const camera_config_t *config);
esp_err_t camera_start_pipeline(camera_pipeline_t *pipeline);
void camera_stop_pipeline(void);
