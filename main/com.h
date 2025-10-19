#pragma once

#include "esp_err.h"

#include "connectivity.h"
#include "image_processing.h"

transport_config_t com_default_config(void);
esp_err_t com_start(const transport_config_t *config, transport_handle_t *handle);
void com_stop(transport_handle_t handle);
void com_stream_packet(transport_handle_t handle, const h264_packet_t *packet);
