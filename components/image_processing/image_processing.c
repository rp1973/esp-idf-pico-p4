#include "image_processing.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include "driver/h264_dma.h"

static const char *TAG = "image_processing";

struct h264_encoder_context_t {
    h264_dma_encoder_handle_t hw_encoder;
    encoder_config_t config;
    uint8_t *bitstream_buffer;
    size_t bitstream_size;
};

encoder_config_t image_processing_default_encoder_config(void)
{
    return (encoder_config_t) {
        .width = 1920,
        .height = 1080,
        .fps = 30,
        .bitrate = 8 * 1024 * 1024,
        .enable_psram = true,
    };
}

esp_err_t image_processing_create_encoder(const encoder_config_t *config, encoder_handle_t *out_handle)
{
    if (!config || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    encoder_handle_t handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }

    handle->config = *config;
    handle->bitstream_size = config->width * config->height * 4;
    handle->bitstream_buffer = heap_caps_malloc(handle->bitstream_size, config->enable_psram ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL);
    if (!handle->bitstream_buffer) {
        free(handle);
        return ESP_ERR_NO_MEM;
    }

    h264_dma_encoder_config_t encoder_config = {
        .width = config->width,
        .height = config->height,
        .frame_rate = config->fps,
        .bit_rate = config->bitrate,
        .profile = H264_PROFILE_HIGH,
    };

    ESP_RETURN_ON_ERROR(h264_dma_new_encoder(&encoder_config, &handle->hw_encoder), TAG, "Failed to create H264 encoder");

    *out_handle = handle;
    return ESP_OK;
}

void image_processing_destroy_encoder(encoder_handle_t handle)
{
    if (!handle) {
        return;
    }
    if (handle->hw_encoder) {
        h264_dma_del_encoder(handle->hw_encoder);
        handle->hw_encoder = NULL;
    }
    if (handle->bitstream_buffer) {
        heap_caps_free(handle->bitstream_buffer);
    }
    free(handle);
}

esp_err_t image_processing_encode_frame(encoder_handle_t handle, const camera_frame_t *frame, h264_packet_t *out_packet)
{
    if (!handle || !frame || !frame->buffer || !out_packet) {
        return ESP_ERR_INVALID_ARG;
    }

    h264_dma_encode_frame_config_t encode_config = {
        .input = frame->buffer,
        .input_size = frame->length,
        .input_format = H264_DMA_INPUT_FORMAT_YUV422,
        .bitstream = handle->bitstream_buffer,
        .bitstream_size = handle->bitstream_size,
        .timestamp = esp_timer_get_time(),
    };

    size_t output_size = 0;
    h264_dma_packet_info_t packet_info = {0};
    esp_err_t err = h264_dma_encode_frame(handle->hw_encoder, &encode_config, &packet_info, &output_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "H264 encode failed: %s", esp_err_to_name(err));
        return err;
    }

    out_packet->data = handle->bitstream_buffer;
    out_packet->length = output_size;
    out_packet->is_keyframe = packet_info.is_idr;
    out_packet->timestamp_us = packet_info.timestamp;

    return ESP_OK;
}

void image_processing_release_packet(encoder_handle_t handle, h264_packet_t *packet)
{
    if (!handle || !packet) {
        return;
    }
    packet->data = NULL;
    packet->length = 0;
}
