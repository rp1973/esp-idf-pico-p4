#include "com.h"

transport_config_t com_default_config(void)
{
    return connectivity_default_transport_config();
}

esp_err_t com_start(const transport_config_t *config, transport_handle_t *handle)
{
    return connectivity_start(config, handle);
}

void com_stop(transport_handle_t handle)
{
    connectivity_stop(handle);
}

void com_stream_packet(transport_handle_t handle, const h264_packet_t *packet)
{
    connectivity_stream_packet(handle, packet);
}
