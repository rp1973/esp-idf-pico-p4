#pragma once

#include <stdint.h>

#include "esp_err.h"

#include "image_processing.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtsp_transport_context_t *transport_handle_t;

typedef enum {
    CONNECTIVITY_TRANSPORT_ETHERNET,
    CONNECTIVITY_TRANSPORT_WIFI,
} transport_type_t;

typedef struct {
    transport_type_t transport_type;
    const char *rtsp_path;
    const char *hostname;
    const char *wifi_ssid;
    const char *wifi_password;
    bool enable_ipv6;
    uint16_t rtsp_port;
} transport_config_t;

esp_err_t connectivity_start(const transport_config_t *config, transport_handle_t *out_handle);
void connectivity_stop(transport_handle_t handle);

transport_config_t connectivity_default_transport_config(void);

esp_err_t connectivity_stream_packet(transport_handle_t handle, const h264_packet_t *packet);

#ifdef __cplusplus
}
#endif
