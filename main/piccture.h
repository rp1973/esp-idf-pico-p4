#pragma once

#include "esp_err.h"

#include "image_processing.h"
#include "camera_driver.h"

encoder_config_t piccture_default_config(void);
esp_err_t piccture_create_encoder(const encoder_config_t *config, encoder_handle_t *handle);
void piccture_destroy_encoder(encoder_handle_t handle);
esp_err_t piccture_encode_frame(encoder_handle_t handle, const camera_frame_t *frame, h264_packet_t *packet);
void piccture_release_packet(encoder_handle_t handle, h264_packet_t *packet);
