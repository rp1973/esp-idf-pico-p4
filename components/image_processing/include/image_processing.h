#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "camera_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct h264_encoder_context_t *encoder_handle_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t bitrate;
    bool enable_psram;
} encoder_config_t;

typedef struct {
    const uint8_t *data;
    size_t length;
    int is_keyframe;
    uint64_t timestamp_us;
} h264_packet_t;

esp_err_t image_processing_create_encoder(const encoder_config_t *config, encoder_handle_t *out_handle);
void image_processing_destroy_encoder(encoder_handle_t handle);

encoder_config_t image_processing_default_encoder_config(void);

esp_err_t image_processing_encode_frame(encoder_handle_t handle, const camera_frame_t *frame, h264_packet_t *out_packet);
void image_processing_release_packet(encoder_handle_t handle, h264_packet_t *packet);

#ifdef __cplusplus
}
#endif
