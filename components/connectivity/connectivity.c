#include "connectivity.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_eth.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "connectivity";

#define CONNECTIVITY_PACKET_QUEUE_LENGTH 4

typedef struct {
    uint8_t *payload;
    size_t length;
    int is_keyframe;
    uint64_t timestamp_us;
} rtsp_frame_packet_t;

struct rtsp_transport_context_t {
    transport_config_t config;
    QueueHandle_t packet_queue;
    TaskHandle_t server_task;
    int client_socket;
    esp_netif_t *netif;
};

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying");
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: %s", ip4addr_ntoa(&event->ip_info.ip));
    }
}

static esp_err_t start_wifi(rtsp_transport_context_t *ctx)
{
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_init_cfg), TAG, "Wi-Fi init failed");

    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL), TAG, "Failed to register Wi-Fi handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL), TAG, "Failed to register IP handler");

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, ctx->config.wifi_ssid ?: "", sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, ctx->config.wifi_password ?: "", sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Failed to set Wi-Fi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "Failed to set Wi-Fi config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Failed to start Wi-Fi");

    return ESP_OK;
}

static void stop_wifi(void)
{
    esp_wifi_stop();
    esp_wifi_deinit();
}

static void rtsp_server_task(void *arg)
{
    rtsp_transport_context_t *ctx = (rtsp_transport_context_t *)arg;
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ctx->config.rtsp_port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    int listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_socket < 0) {
        ESP_LOGE(TAG, "Failed to create RTSP socket");
        vTaskDelete(NULL);
        return;
    }

    int enable = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    if (bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind RTSP socket");
        close(listen_socket);
        vTaskDelete(NULL);
        return;
    }

    listen(listen_socket, 1);
    ESP_LOGI(TAG, "RTSP server listening on rtsp://%s:%d%s", ctx->config.hostname, ctx->config.rtsp_port, ctx->config.rtsp_path);

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(listen_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            continue;
        }

        ESP_LOGI(TAG, "RTSP client connected: %s", inet_ntoa(client_addr.sin_addr));
        ctx->client_socket = client_socket;

        while (true) {
            rtsp_frame_packet_t packet;
            if (xQueueReceive(ctx->packet_queue, &packet, portMAX_DELAY) != pdTRUE) {
                continue;
            }

            if (ctx->client_socket >= 0) {
                int sent = send(ctx->client_socket, packet.payload, packet.length, 0);
                if (sent < 0) {
                    ESP_LOGW(TAG, "Client disconnected");
                    close(ctx->client_socket);
                    ctx->client_socket = -1;
                    free(packet.payload);
                    break;
                }
            }

            free(packet.payload);
        }
    }
}

static esp_err_t start_network(rtsp_transport_context_t *ctx)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif init failed");

    if (ctx->config.transport_type == CONNECTIVITY_TRANSPORT_ETHERNET) {
        ctx->netif = esp_netif_create_default_eth_netif();
        ESP_LOGI(TAG, "Created default Ethernet netif");
    } else {
        ctx->netif = esp_netif_create_default_wifi_sta();
        ESP_RETURN_ON_ERROR(start_wifi(ctx), TAG, "Failed to start Wi-Fi");
    }

    return ESP_OK;
}

static void stop_network(rtsp_transport_context_t *ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->config.transport_type == CONNECTIVITY_TRANSPORT_WIFI) {
        stop_wifi();
    }
    if (ctx->netif) {
        esp_netif_destroy(ctx->netif);
        ctx->netif = NULL;
    }
}

transport_config_t connectivity_default_transport_config(void)
{
    return (transport_config_t) {
        .transport_type = CONNECTIVITY_TRANSPORT_ETHERNET,
        .rtsp_path = "/stream",
        .hostname = "esp32-p4",
        .wifi_ssid = "",
        .wifi_password = "",
        .enable_ipv6 = true,
        .rtsp_port = 8554,
    };
}

esp_err_t connectivity_start(const transport_config_t *config, transport_handle_t *out_handle)
{
    if (!config || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    rtsp_transport_context_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }

    ctx->config = *config;
    ctx->client_socket = -1;
    ctx->packet_queue = xQueueCreate(CONNECTIVITY_PACKET_QUEUE_LENGTH, sizeof(rtsp_frame_packet_t));
    if (!ctx->packet_queue) {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    ESP_RETURN_ON_ERROR(start_network(ctx), TAG, "Failed to start network");

    BaseType_t task_created = xTaskCreatePinnedToCore(rtsp_server_task, "rtsp_server", 6 * 1024, ctx, tskIDLE_PRIORITY + 4, &ctx->server_task, tskNO_AFFINITY);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RTSP server task");
        stop_network(ctx);
        vQueueDelete(ctx->packet_queue);
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    *out_handle = ctx;
    return ESP_OK;
}

void connectivity_stop(transport_handle_t handle)
{
    if (!handle) {
        return;
    }

    rtsp_transport_context_t *ctx = handle;
    if (ctx->server_task) {
        vTaskDelete(ctx->server_task);
        ctx->server_task = NULL;
    }
    if (ctx->client_socket >= 0) {
        close(ctx->client_socket);
        ctx->client_socket = -1;
    }
    if (ctx->packet_queue) {
        rtsp_frame_packet_t packet;
        while (xQueueReceive(ctx->packet_queue, &packet, 0) == pdTRUE) {
            free(packet.payload);
        }
        vQueueDelete(ctx->packet_queue);
        ctx->packet_queue = NULL;
    }
    stop_network(ctx);
    free(ctx);
}

esp_err_t connectivity_stream_packet(transport_handle_t handle, const h264_packet_t *packet)
{
    if (!handle || !packet || !packet->data || packet->length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    rtsp_transport_context_t *ctx = handle;
    rtsp_frame_packet_t frame_packet = {
        .length = packet->length,
        .is_keyframe = packet->is_keyframe,
        .timestamp_us = packet->timestamp_us,
        .payload = malloc(packet->length),
    };

    if (!frame_packet.payload) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(frame_packet.payload, packet->data, packet->length);

    if (xQueueSend(ctx->packet_queue, &frame_packet, pdMS_TO_TICKS(10)) != pdTRUE) {
        free(frame_packet.payload);
        ESP_LOGW(TAG, "Dropping packet due to full queue");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}
