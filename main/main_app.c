#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "camera_driver.h"
#include "image_processing.h"
#include "connectivity.h"

static const char *TAG = "main";

typedef struct {
    encoder_handle_t encoder;
    transport_handle_t transport;
} camera_pipeline_handle_t;

static void camera_task(void *arg)
{
    camera_pipeline_handle_t *pipeline = (camera_pipeline_handle_t *)arg;

    while (true) {
        camera_frame_t frame = {0};
        if (camera_driver_acquire_frame(&frame, pdMS_TO_TICKS(1000)) == ESP_OK) {
            h264_packet_t packet = {0};
            if (image_processing_encode_frame(pipeline->encoder, &frame, &packet) == ESP_OK) {
                connectivity_stream_packet(pipeline->transport, &packet);
                image_processing_release_packet(pipeline->encoder, &packet);
            } else {
                ESP_LOGW(TAG, "Failed to encode frame");
            }
            camera_driver_release_frame(&frame);
        } else {
            ESP_LOGW(TAG, "Timeout waiting for camera frame");
        }
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    camera_config_t camera_cfg = camera_driver_default_config();
    ESP_ERROR_CHECK(camera_driver_init(&camera_cfg));

    encoder_config_t encoder_cfg = image_processing_default_encoder_config();
    transport_config_t transport_cfg = connectivity_default_transport_config();

    camera_pipeline_handle_t pipeline = {0};
    ESP_ERROR_CHECK(image_processing_create_encoder(&encoder_cfg, &pipeline.encoder));
    ESP_ERROR_CHECK(connectivity_start(&transport_cfg, &pipeline.transport));

    BaseType_t task_created = xTaskCreatePinnedToCore(
        camera_task,
        "camera_task",
        8 * 1024,
        &pipeline,
        tskIDLE_PRIORITY + 5,
        NULL,
        tskNO_AFFINITY);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create camera task");
        connectivity_stop(pipeline.transport);
        image_processing_destroy_encoder(pipeline.encoder);
    }
}
