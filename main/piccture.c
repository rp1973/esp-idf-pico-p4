#include "piccture.h"

encoder_config_t piccture_default_config(void)
{
    return image_processing_default_encoder_config();
}

esp_err_t piccture_create_encoder(const encoder_config_t *config, encoder_handle_t *handle)
{
    return image_processing_create_encoder(config, handle);
}

void piccture_destroy_encoder(encoder_handle_t handle)
{
    image_processing_destroy_encoder(handle);
}

esp_err_t piccture_encode_frame(encoder_handle_t handle, const camera_frame_t *frame, h264_packet_t *packet)
{
    return image_processing_encode_frame(handle, frame, packet);
}

void piccture_release_packet(encoder_handle_t handle, h264_packet_t *packet)
{
    image_processing_release_packet(handle, packet);
}
