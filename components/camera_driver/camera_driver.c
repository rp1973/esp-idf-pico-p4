#include "camera_driver.h"

#include <inttypes.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/csi.h"

static const char *TAG = "camera_driver";

static QueueHandle_t s_available_frames;
static QueueHandle_t s_ready_frames;
static csi_device_handle_t s_csi_handle;
static camera_config_t s_camera_config;

static esp_err_t allocate_frame_buffers(void)
{
    const size_t buffer_size = s_camera_config.width * s_camera_config.height * 2;

    for (uint32_t i = 0; i < s_camera_config.frame_buffer_count; ++i) {
        uint8_t *buffer = (uint8_t *)heap_caps_calloc(1, buffer_size, MALLOC_CAP_SPIRAM);
        if (!buffer) {
            ESP_LOGE(TAG, "Failed to allocate frame buffer %" PRIu32, i);
            return ESP_ERR_NO_MEM;
        }
        camera_frame_t frame = {
            .buffer = buffer,
            .length = buffer_size,
            .width = s_camera_config.width,
            .height = s_camera_config.height,
            .pixel_format = s_camera_config.pixel_format,
        };
        if (xQueueSend(s_available_frames, &frame, 0) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to populate frame queue");
            heap_caps_free(buffer);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static bool csi_frame_ready_callback(const csi_frame_buffer_t *buffer, void *user_ctx)
{
    camera_frame_t frame = {0};
    if (xQueueReceiveFromISR(s_available_frames, &frame, NULL) != pdTRUE) {
        return false;
    }

    memcpy(frame.buffer, buffer->buffer, frame.length);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR((QueueHandle_t)user_ctx, &frame, &xHigherPriorityTaskWoken);
    return xHigherPriorityTaskWoken == pdTRUE;
}

esp_err_t camera_driver_init(const camera_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    s_camera_config = *config;

    if (!s_available_frames) {
        s_available_frames = xQueueCreate(config->frame_buffer_count, sizeof(camera_frame_t));
        if (!s_available_frames) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (!s_ready_frames) {
        s_ready_frames = xQueueCreate(config->frame_buffer_count, sizeof(camera_frame_t));
        if (!s_ready_frames) {
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_RETURN_ON_ERROR(allocate_frame_buffers(), TAG, "Failed to allocate frame buffers");

    csi_config_t csi_config = {
        .width = config->width,
        .height = config->height,
        .pixformat = config->pixel_format,
        .xclk_io_num = config->xclk_pin,
        .vsync_io_num = config->vsync_pin,
        .href_io_num = config->href_pin,
        .pclk_io_num = config->pclk_pin,
        .data_io_num = {
            config->data.d0,
            config->data.d1,
            config->data.d2,
            config->data.d3,
            config->data.d4,
            config->data.d5,
            config->data.d6,
            config->data.d7,
        },
        .frame_buffer_count = config->frame_buffer_count,
        .frame_buffer_size = config->width * config->height * 2,
        .flags = {
            .double_speed = false,
        },
    };

    ESP_RETURN_ON_ERROR(csi_new_device(&csi_config, &s_csi_handle), TAG, "CSI new device failed");

    csi_frame_buffer_event_callbacks_t callbacks = {
        .on_frame_ready = csi_frame_ready_callback,
        .user_ctx = s_ready_frames,
    };
    ESP_RETURN_ON_ERROR(csi_register_frame_buffer_event_callbacks(s_csi_handle, &callbacks), TAG, "CSI callback registration failed");

    return ESP_OK;
}

void camera_driver_deinit(void)
{
    if (s_csi_handle) {
        csi_del_device(s_csi_handle);
        s_csi_handle = NULL;
    }
    if (s_available_frames) {
        camera_frame_t frame;
        while (xQueueReceive(s_available_frames, &frame, 0) == pdTRUE) {
            heap_caps_free(frame.buffer);
        }
        vQueueDelete(s_available_frames);
        s_available_frames = NULL;
    }
    if (s_ready_frames) {
        camera_frame_t frame;
        while (xQueueReceive(s_ready_frames, &frame, 0) == pdTRUE) {
            heap_caps_free(frame.buffer);
        }
        vQueueDelete(s_ready_frames);
        s_ready_frames = NULL;
    }
}

camera_config_t camera_driver_default_config(void)
{
    return (camera_config_t) {
        .width = 1920,
        .height = 1080,
        .pixel_format = PIXFORMAT_YUV422,
        .frame_buffer_count = 3,
        .xclk_pin = GPIO_NUM_40,
        .vsync_pin = GPIO_NUM_41,
        .href_pin = GPIO_NUM_42,
        .pclk_pin = GPIO_NUM_39,
        .data = {
            .d0 = GPIO_NUM_0,
            .d1 = GPIO_NUM_1,
            .d2 = GPIO_NUM_2,
            .d3 = GPIO_NUM_3,
            .d4 = GPIO_NUM_4,
            .d5 = GPIO_NUM_5,
            .d6 = GPIO_NUM_6,
            .d7 = GPIO_NUM_7,
        },
    };
}

esp_err_t camera_driver_acquire_frame(camera_frame_t *frame, TickType_t ticks_to_wait)
{
    if (!frame || !s_ready_frames) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueReceive(s_ready_frames, frame, ticks_to_wait) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

void camera_driver_release_frame(camera_frame_t *frame)
{
    if (!frame || !frame->buffer || !s_available_frames) {
        return;
    }
    xQueueSend(s_available_frames, frame, portMAX_DELAY);
}
