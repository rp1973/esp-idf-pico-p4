#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "driver/csi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t width;
    uint32_t height;
    pixformat_t pixel_format;
    uint32_t frame_buffer_count;
    gpio_num_t xclk_pin;
    gpio_num_t vsync_pin;
    gpio_num_t href_pin;
    gpio_num_t pclk_pin;
    struct {
        gpio_num_t d0;
        gpio_num_t d1;
        gpio_num_t d2;
        gpio_num_t d3;
        gpio_num_t d4;
        gpio_num_t d5;
        gpio_num_t d6;
        gpio_num_t d7;
    } data;
} camera_config_t;

typedef struct {
    uint8_t *buffer;
    size_t length;
    uint32_t width;
    uint32_t height;
    pixformat_t pixel_format;
} camera_frame_t;

esp_err_t camera_driver_init(const camera_config_t *config);
void camera_driver_deinit(void);

camera_config_t camera_driver_default_config(void);

esp_err_t camera_driver_acquire_frame(camera_frame_t *frame, TickType_t ticks_to_wait);
void camera_driver_release_frame(camera_frame_t *frame);

#ifdef __cplusplus
}
#endif
