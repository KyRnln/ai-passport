// main/pc_client.c —— AIDA64 RemoteSensor SSE 客户端实现。
#include "pc_client.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "pc_client";

#define CLIENT_STACK 6144
#define CLIENT_TASK_PRIO 4
#define RETRY_DELAY_MS 3000
#define HTTP_TIMEOUT_MS 15000
#define STOP_WAIT_MS 3000

static TaskHandle_t s_task;
static volatile bool s_running;

// 快照数据的短临界区保护(UI 定时器与客户端任务之间)。
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static pc_metrics_t s_metrics;
static pc_client_state_t s_state = PC_CLIENT_IDLE;
static int64_t s_last_update_ms;

// 保护 http client 句柄生命周期:停止时需要从外部主动 close 才能让
// 阻塞中的 esp_http_client_perform() 返回。
static SemaphoreHandle_t s_client_mutex;
static esp_http_client_handle_t s_client;

static pc_config_t s_cfg;
static char s_url[128];
static pc_metrics_parser_t s_parser;

static void set_state(pc_client_state_t state)
{
    portENTER_CRITICAL(&s_lock);
    s_state = state;
    portEXIT_CRITICAL(&s_lock);
}

static void publish_metrics(void)
{
    const int64_t now_ms = esp_timer_get_time() / 1000;
    portENTER_CRITICAL(&s_lock);
    s_metrics = s_parser.metrics;
    s_last_update_ms = now_ms;
    portEXIT_CRITICAL(&s_lock);
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        set_state(PC_CLIENT_STREAMING);
        break;
    case HTTP_EVENT_ON_DATA:
        if (evt->data != NULL && evt->data_len > 0 &&
            pc_metrics_parser_feed(&s_parser, (const char *)evt->data,
                                   (size_t)evt->data_len)) {
            publish_metrics();
        }
        break;
    case HTTP_EVENT_DISCONNECTED:
        set_state(PC_CLIENT_RETRYING);
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGW(TAG, "连接出错");
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void client_task(void *arg)
{
    (void)arg;

    pc_metrics_parser_init(&s_parser);
    snprintf(s_url, sizeof(s_url), "http://%s:%u/sse", s_cfg.host,
             (unsigned)s_cfg.port);
    ESP_LOGI(TAG, "开始连接 %s", s_url);

    while (s_running) {
        set_state(PC_CLIENT_CONNECTING);

        const esp_http_client_config_t config = {
            .url = s_url,
            .event_handler = http_event_handler,
            .timeout_ms = HTTP_TIMEOUT_MS,
            .keep_alive_enable = true,
            .buffer_size = 1024,
            .buffer_size_tx = 512,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == NULL) {
            ESP_LOGE(TAG, "创建 HTTP 客户端失败");
            set_state(PC_CLIENT_RETRYING);
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            continue;
        }

        esp_http_client_set_method(client, HTTP_METHOD_GET);
        esp_http_client_set_header(client, "Accept", "text/event-stream");
        esp_http_client_set_header(client, "Cache-Control", "no-cache");

        xSemaphoreTake(s_client_mutex, portMAX_DELAY);
        s_client = client;
        const bool aborted = !s_running;
        xSemaphoreGive(s_client_mutex);

        esp_err_t err = ESP_ERR_INVALID_STATE;
        if (!aborted) {
            err = esp_http_client_perform(client);
        }

        xSemaphoreTake(s_client_mutex, portMAX_DELAY);
        s_client = NULL;
        xSemaphoreGive(s_client_mutex);
        esp_http_client_cleanup(client);

        if (!s_running) {
            break;
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "SSE 连接结束: %s", esp_err_to_name(err));
        }
        set_state(PC_CLIENT_RETRYING);
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }

    set_state(PC_CLIENT_IDLE);
    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t pc_client_start(const pc_config_t *cfg)
{
    if (cfg == NULL || !pc_config_complete(cfg)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_client_mutex == NULL) {
        s_client_mutex = xSemaphoreCreateMutex();
        if (s_client_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    s_cfg = *cfg;
    pc_metrics_parser_init(&s_parser);

    portENTER_CRITICAL(&s_lock);
    memset(&s_metrics, 0, sizeof(s_metrics));
    s_state = PC_CLIENT_CONNECTING;
    s_last_update_ms = 0;
    portEXIT_CRITICAL(&s_lock);

    s_running = true;
    if (xTaskCreate(client_task, "pc_sse", CLIENT_STACK, NULL, CLIENT_TASK_PRIO,
                    &s_task) != pdPASS) {
        s_running = false;
        s_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void pc_client_stop(void)
{
    s_running = false;

    if (s_client_mutex != NULL) {
        xSemaphoreTake(s_client_mutex, portMAX_DELAY);
        if (s_client != NULL) {
            (void)esp_http_client_close(s_client);
        }
        xSemaphoreGive(s_client_mutex);
    }

    for (int waited = 0; waited < STOP_WAIT_MS && s_task != NULL; waited += 20) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (s_task != NULL) {
        ESP_LOGW(TAG, "客户端任务未在 %d ms 内退出", STOP_WAIT_MS);
    }
}

void pc_client_snapshot(pc_metrics_t *metrics, pc_client_state_t *state,
                        int64_t *last_update_ms)
{
    portENTER_CRITICAL(&s_lock);
    if (metrics != NULL) {
        *metrics = s_metrics;
    }
    if (state != NULL) {
        *state = s_state;
    }
    if (last_update_ms != NULL) {
        *last_update_ms = s_last_update_ms;
    }
    portEXIT_CRITICAL(&s_lock);
}
