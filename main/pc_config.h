// main/pc_config.h —— 应用配置(NVS 持久化)。
//
// 保存 Wi-Fi STA 凭证与 AIDA64 主机地址。HTTP 表单提交属于外部输入,
// 字段长度上限在此定义,BSP/网络层不得各自再定一套。
// 另存屏幕背光亮度:属于本机 UI 设置,放在独立命名空间,与网络配置互不影响。
#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

#define PC_CFG_SSID_MAX 33   // 32 字节 SSID + NUL
#define PC_CFG_PASS_MAX 65   // WPA2 最长 63 字节 + NUL
#define PC_CFG_HOST_MAX 64   // IPv4/主机名
#define PC_CFG_PORT_DEFAULT 80

// 默认背光 60%:屏幕背光是整机最稳定的耗电项,满亮度(100%)对续航影响明显。
// 仅作默认值,设备上仍可通过设置页在 10%~100% 之间调节。
#define PC_CFG_BRIGHTNESS_DEFAULT 60
#define PC_CFG_BRIGHTNESS_MIN 10     // 最低 10%,避免调到全黑看不见
#define PC_CFG_BRIGHTNESS_MAX 100
#define PC_CFG_BRIGHTNESS_STEP 10    // 每次调整的档位

typedef struct {
    char ssid[PC_CFG_SSID_MAX];
    char password[PC_CFG_PASS_MAX];
    char host[PC_CFG_HOST_MAX];
    uint16_t port;
} pc_config_t;

// 置为"未配置"状态(仅端口使用默认值)。
void pc_config_set_defaults(pc_config_t *cfg);

// SSID 与 AIDA64 主机地址都非空才算配置完整。密码允许为空(开放网络)。
bool pc_config_complete(const pc_config_t *cfg);

// 读取已保存配置。尚无记录时返回 ESP_ERR_NOT_FOUND;NVS 不可用时返回其它错误。
esp_err_t pc_config_load(pc_config_t *cfg);

// 覆盖保存。仅在 STA 连接验证成功后才应调用,避免写坏可用的旧配置。
esp_err_t pc_config_save(const pc_config_t *cfg);

// 清除已保存配置(用于"忘记网络")。
esp_err_t pc_config_clear(void);

// 读取/保存屏幕背光亮度(%,PC_CFG_BRIGHTNESS_MIN..100)。无记录时
// pc_config_load_brightness 返回 ESP_ERR_NOT_FOUND,并把 out 置为默认值。
esp_err_t pc_config_load_brightness(uint8_t *percent);
esp_err_t pc_config_save_brightness(uint8_t percent);

// 读取/保存横屏方向:<false> 为默认方向,<true> 为 180° 翻转。
// 无记录时 pc_config_load_flipped 返回 ESP_ERR_NOT_FOUND,并把 out 置为 false。
esp_err_t pc_config_load_flipped(bool *flipped);
esp_err_t pc_config_save_flipped(bool flipped);
