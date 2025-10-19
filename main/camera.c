#include "camera.h"

#include "esp_log.h"

static const char *TAG = "camera";

static TaskHandle_t s_camera_task_handle = NULL;

static void camera_task(void *arg)
{
    camera_pipeline_t *pipeline = (camera_pipeline_t *)arg;

    if (pipeline == NULL) {
        ESP_LOGE(TAG, "Kein Pipeline-Kontext verfügbar, Kamera-Task beendet");
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        camera_frame_t frame = {0};
        if (camera_driver_acquire_frame(&frame, pdMS_TO_TICKS(1000)) == ESP_OK) {
            h264_packet_t packet = {0};
            if (piccture_encode_frame(pipeline->encoder, &frame, &packet) == ESP_OK) {
                com_stream_packet(pipeline->transport, &packet);
                piccture_release_packet(pipeline->encoder, &packet);
            } else {
                ESP_LOGW(TAG, "Kodierung des Frames fehlgeschlagen");
            }
            camera_driver_release_frame(&frame);
        } else {
            ESP_LOGW(TAG, "Zeitüberschreitung beim Warten auf Kamerabild");
        }
    }
}

camera_config_t camera_default_config(void)
{
    return camera_driver_default_config();
}

esp_err_t camera_driver_setup(const camera_config_t *config)
{
    return camera_driver_init(config);
}

esp_err_t camera_start_pipeline(camera_pipeline_t *pipeline)
{
    if (pipeline == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_camera_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t task_created = xTaskCreatePinnedToCore(
        camera_task,
        "camera_task",
        8 * 1024,
        pipeline,
        tskIDLE_PRIORITY + 5,
        &s_camera_task_handle,
        tskNO_AFFINITY);

    if (task_created != pdPASS) {
        s_camera_task_handle = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}

void camera_stop_pipeline(void)
{
    if (s_camera_task_handle != NULL) {
        vTaskDelete(s_camera_task_handle);
        s_camera_task_handle = NULL;
    }
}
