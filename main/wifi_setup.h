// main/wifi_setup.h —— Wi-Fi 连接与 SoftAP 网页配网。
//
// 两种互斥的运行态:
//   监控态:Wi-Fi 纯 STA 模式,连接已保存的网络。
//   配网态:APSTA 模式,开热点 + 本地配置网页,页面提交后即时验证 STA 连接,
//           只有验证成功才写 NVS(避免一次手误把可用配置写坏)。
//
// 生命周期由应用层显式控制:先 wifi_setup_prepare() 一次,之后按需
// wifi_sta_connect() 或 wifi_setup_ap_start()/wifi_setup_ap_stop()。
#pragma once

#include "esp_err.h"
#include "pc_config.h"

#include <stdbool.h>
#include <stdint.h>

#define WIFI_SETUP_AP_SSID "AIPassport-Setup"
#define WIFI_SETUP_AP_PASS "aipassport"      // 至少 8 位,避免开放热点
#define WIFI_SETUP_AP_URL "http://192.168.4.1"
#define WIFI_SETUP_STA_VERIFY_MS 15000       // 提交表单后等待 STA 拿到 IP 的上限

// 初始化 netif / 事件循环 / Wi-Fi 驱动,并以 STA 模式启动(不连接)。
// 幂等;NVS 由调用方(main)负责初始化。
esp_err_t wifi_setup_prepare(void);

// 发起 STA 连接。非阻塞;断开时自动重连,直到 wifi_setup_sta_disconnect()。
esp_err_t wifi_setup_sta_connect(const pc_config_t *cfg);

// 停止 STA 并关闭自动重连。
void wifi_setup_sta_disconnect(void);

// 阻塞等待 STA 拿到 IP,上限 timeout_ms。仅可在非 LVGL、非按键回调的任务中调用。
bool wifi_setup_sta_wait_connected(uint32_t timeout_ms);

// 当前是否已拿到 IP。
bool wifi_setup_sta_has_ip(void);

// 进入配网态:切换到 APSTA、开热点、启动配置网页。
esp_err_t wifi_setup_ap_start(void);

// 退出配网态:停止网页与热点,回到纯 STA 模式(不自动重连)。
void wifi_setup_ap_stop(void);

bool wifi_setup_ap_active(void);

// 配网页面是否已成功保存并验证了新配置。应用层据此切回监控态。
bool wifi_setup_configured(void);
void wifi_setup_clear_configured(void);
