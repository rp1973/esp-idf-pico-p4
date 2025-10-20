#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include <stdlib.h>

#include "camera.h"
#include "picture.h"
#include "com.h"

static const char *TAG = "main";

static camera_pipeline_t *s_pipeline = NULL;

static void pipeline_shutdown_handler(void)
{
    if (s_pipeline == NULL) {
        return;
    }

    camera_stop_pipeline();
    if (s_pipeline->transport) {
        com_stop(s_pipeline->transport);
        s_pipeline->transport = NULL;
    }
    if (s_pipeline->encoder) {
        piccture_destroy_encoder(s_pipeline->encoder);
        s_pipeline->encoder = NULL;
    }
    free(s_pipeline);
    s_pipeline = NULL;
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

    camera_config_t camera_cfg = camera_default_config();
    ESP_ERROR_CHECK(camera_driver_setup(&camera_cfg));

    encoder_config_t encoder_cfg = piccture_default_config();
    transport_config_t transport_cfg = com_default_config();

    camera_pipeline_t *pipeline = calloc(1, sizeof(*pipeline));
    if (!pipeline) {
        ESP_LOGE(TAG, "Speicherreservierung für Kamerapipeline fehlgeschlagen");
        return;
    }

    ESP_ERROR_CHECK(piccture_create_encoder(&encoder_cfg, &pipeline->encoder));
    esp_err_t com_ret = com_start(&transport_cfg, &pipeline->transport);
    if (com_ret != ESP_OK) {
        ESP_LOGE(TAG, "Start der Netzwerkübertragung fehlgeschlagen");
        piccture_destroy_encoder(pipeline->encoder);
        free(pipeline);
        ESP_ERROR_CHECK(com_ret);
    }

    esp_err_t camera_ret = camera_start_pipeline(pipeline);
    if (camera_ret != ESP_OK) {
        ESP_LOGE(TAG, "Start der Kameraufgabe fehlgeschlagen");
        com_stop(pipeline->transport);
        piccture_destroy_encoder(pipeline->encoder);
        free(pipeline);
        ESP_LOGW(TAG, "Fehlercode Kamerastart: %s", esp_err_to_name(camera_ret));
        return;
    }

    s_pipeline = pipeline;
    ESP_ERROR_CHECK(esp_register_shutdown_handler(pipeline_shutdown_handler));
}
