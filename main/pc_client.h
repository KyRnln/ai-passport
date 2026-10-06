// main/pc_client.h —— AIDA64 RemoteSensor SSE 客户端。
//
// 在独立任务里维持一条 GET http://<host>:<port>/sse 长连接,把解析结果
// 快照出来供 UI 读取。UI 只通过 pc_client_snapshot() 取值,不直接触碰
// 客户端内部状态,避免跨任务访问 LVGL 对象。
#pragma once

#include "esp_err.h"
#include "pc_config.h"
#include "pc_metrics.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PC_CLIENT_IDLE = 0,
    PC_CLIENT_CONNECTING,
    PC_CLIENT_STREAMING,
    PC_CLIENT_RETRYING,
} pc_client_state_t;

// 启动持续采集。配置必须完整(SSID/host 非空);重复调用返回 ESP_ERR_INVALID_STATE。
esp_err_t pc_client_start(const pc_config_t *cfg);

// 停止采集并等待任务退出。可从任意非 LVGL 任务调用。
void pc_client_stop(void);

// 读取最新快照(线程安全)。任一输出参数可传 NULL。
void pc_client_snapshot(pc_metrics_t *metrics, pc_client_state_t *state,
                        int64_t *last_update_ms);
