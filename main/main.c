// main/main.c —— AI Passport PC Monitor 应用入口。
//
// 这是派生应用:启动后直接进入自有的监控界面,不使用基线 demo 的测试菜单
// 与 ui_pixel 视觉外壳(BSP 驱动与并发模式仍然复用)。
//
// 按键语义(本应用自定义):
//   监控主页   确定长按   进入设置(短按不动作)
//   设置页     上下短按   选择条目(仅选中项有高亮边框)
//              确定短按   进入条目 / 结束亮度调节
//              确定长按   返回监控主页
//   亮度条目   上键调亮、下键调暗;10 档步进(范围 10..100)
//   Wi-Fi 条目 确定短按   二次确认后才进入配网
//   配网态     确定长按   退出配网
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "pc_client.h"
#include "pc_config.h"
#include "pc_ui.h"
#include "wifi_setup.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "app";

// 事件位。显式写成移位表达式,避免依赖 BIT0 这类平台宏。
#define REQ_MONITOR (1u << 0)
#define REQ_SETUP (1u << 1)
#define INPUT_QUEUE_DEPTH 8
#define WIFI_WAIT_MS 20000

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static EventGroupHandle_t s_requests;
static QueueHandle_t s_input_queue;
static TaskHandle_t s_input_task;
static volatile bool s_input_ready;
static volatile bool s_in_setup;
static bool s_monitoring;

static pc_config_t s_active_cfg;

// ---------------------------------------------------------------------------
// NVS
// ---------------------------------------------------------------------------
static esp_err_t storage_init(void)
{
    const esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 与仓库基线一致:不自动擦除,避免丢掉用户已保存的配置。
        ESP_LOGE(TAG, "NVS 需要重建(%s);请擦除 NVS 分区后重启设备",
                 esp_err_to_name(err));
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS 初始化失败: %s", esp_err_to_name(err));
    }
    return err;
}

// ---------------------------------------------------------------------------
// 运行模式
// ---------------------------------------------------------------------------
static void enter_monitor_mode(void);

static void enter_setup_mode(void)
{
    if (s_monitoring) {
        pc_client_stop();
        wifi_setup_sta_disconnect();
        s_monitoring = false;
    }

    s_in_setup = true;

    const esp_err_t err = wifi_setup_ap_start();
    if (err != ESP_OK) {
        s_in_setup = false;
        pc_ui_show_message("无法启动配网",
                           "无法开启配网热点。请重启设备后重试。");
        return;
    }

    wifi_setup_clear_configured();
    pc_ui_show_setup();

    // 等待网页提交成功,或用户长按确定键退出配网。
    for (;;) {
        const EventBits_t bits = xEventGroupWaitBits(s_requests, REQ_MONITOR, pdTRUE,
                                                     pdFALSE, pdMS_TO_TICKS(500));
        if ((bits & REQ_MONITOR) != 0 || wifi_setup_configured()) {
            break;
        }
    }

    wifi_setup_ap_stop();
    wifi_setup_clear_configured();
    s_in_setup = false;

    pc_config_t cfg;
    if (pc_config_load(&cfg) == ESP_OK && pc_config_complete(&cfg)) {
        s_active_cfg = cfg;
        pc_ui_set_config(&cfg);
        enter_monitor_mode();
    } else {
        pc_ui_show_message("未完成配置",
                           "没有已保存的 Wi-Fi / AIDA64 设置。长按 OK 重新配网。");
    }
}

static void enter_monitor_mode(void)
{
    if (!pc_config_complete(&s_active_cfg)) {
        pc_ui_show_message("未完成配置",
                           "长按 OK 连接 Wi-Fi 并填写 PC 地址。");
        return;
    }

    s_monitoring = true;
    pc_ui_show_monitor();

    uint32_t attempts = 0;
    while (!wifi_setup_sta_has_ip() && attempts < 3) {
        attempts++;
        if (wifi_setup_sta_connect(&s_active_cfg) != ESP_OK) {
            break;
        }
        if (wifi_setup_sta_wait_connected(WIFI_WAIT_MS)) {
            break;
        }
        ESP_LOGW(TAG, "第 %u 次连接 Wi-Fi 超时", (unsigned)attempts);
    }

    if (!wifi_setup_sta_has_ip()) {
        s_monitoring = false;
        pc_ui_show_message("Wi-Fi 未连接",
                           "请确认网络为 2.4GHz 且在范围内, 再长按 OK 重新配网。");
        return;
    }

    const esp_err_t err = pc_client_start(&s_active_cfg);
    if (err != ESP_OK) {
        s_monitoring = false;
        pc_ui_show_message("监控启动失败",
                           "无法启动 AIDA64 客户端。长按 OK 重新配置。");
        return;
    }

    wifi_setup_clear_configured();
    ESP_LOGI(TAG, "监控已启动: PC=%s:%u", s_active_cfg.host,
             (unsigned)s_active_cfg.port);
}

static void app_task(void *arg)
{
    (void)arg;

    pc_config_t cfg;
    const esp_err_t err = pc_config_load(&cfg);
    if (err == ESP_OK && pc_config_complete(&cfg)) {
        s_active_cfg = cfg;
        pc_ui_set_config(&cfg);
        xEventGroupSetBits(s_requests, REQ_MONITOR);
    } else {
        ESP_LOGI(TAG, "尚无有效配置,进入配网模式");
        xEventGroupSetBits(s_requests, REQ_SETUP);
    }

    for (;;) {
        const EventBits_t bits = xEventGroupWaitBits(
            s_requests, REQ_MONITOR | REQ_SETUP, pdTRUE, pdFALSE, portMAX_DELAY);
        if ((bits & REQ_SETUP) != 0) {
            enter_setup_mode();
        } else if ((bits & REQ_MONITOR) != 0) {
            enter_monitor_mode();
        }
    }
}

// ---------------------------------------------------------------------------
// 按键分发
// ---------------------------------------------------------------------------
static void process_input(const input_event_t *input)
{
    // 配网态只显示配网引导页:长按确定退出,其余按键不参与界面导航。
    if (s_in_setup) {
        if (input->event == BSP_BTN_LONG && input->btn == BSP_BTN_OK) {
            xEventGroupSetBits(s_requests, REQ_MONITOR);
        }
        return;
    }

    if (input->event != BSP_BTN_CLICK && input->event != BSP_BTN_LONG) {
        return;
    }
    const bool is_long = (input->event == BSP_BTN_LONG);

    // 界面导航全部由 UI 状态机决定;它只会向应用层请求"进入配网"这一个动作。
    pc_ui_action_t action = PC_UI_ACTION_NONE;
    switch (input->btn) {
    case BSP_BTN_UP:
        action = pc_ui_on_up(is_long);
        break;
    case BSP_BTN_DOWN:
        action = pc_ui_on_down(is_long);
        break;
    case BSP_BTN_OK:
        action = pc_ui_on_ok(is_long);
        break;
    default:
        break;
    }

    if (action == PC_UI_ACTION_ENTER_SETUP) {
        xEventGroupSetBits(s_requests, REQ_SETUP);
    }
}

static void input_task(void *arg)
{
    (void)arg;
    input_event_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, portMAX_DELAY) == pdTRUE) {
            process_input(&input);
        }
    }
}

// 按键回调运行在共享的 esp_timer 任务上:只入队,立即返回。
static void on_key(bsp_btn_t btn, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!s_input_ready || s_input_queue == NULL) {
        return;
    }
    const input_event_t input = { .btn = btn, .event = event };
    (void)xQueueSend(s_input_queue, &input, 0);
}

// ---------------------------------------------------------------------------
// 入口
// ---------------------------------------------------------------------------
void app_main(void)
{
    ESP_LOGI(TAG, "AI Passport PC Monitor 启动");
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "休眠唤醒,继续运行监控应用");
    }

    bsp_i2c_init();

    if (bsp_display_init() != ESP_OK || bsp_lvgl_init() == NULL) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败,应用无法启动。"
                      "检查 SPI 接线(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    // 先用默认亮度点亮,pc_ui_init() 随后按 NVS 里保存的档位覆盖。
    bsp_display_backlight(PC_CFG_BRIGHTNESS_DEFAULT);

    // 外设单项失败不阻塞:电量读不到时界面不显示数字。
    (void)bsp_battery_init();

    if (storage_init() != ESP_OK) {
        ESP_LOGW(TAG, "配置存储不可用;本次运行的配置不会保存");
    }

    s_requests = xEventGroupCreate();
    if (s_requests == NULL) {
        ESP_LOGE(TAG, "创建事件组失败");
        return;
    }

    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (s_input_queue == NULL) {
        ESP_LOGE(TAG, "创建按键队列失败");
        return;
    }
    if (xTaskCreate(input_task, "app_input", 4096, NULL, 5, &s_input_task) != pdPASS) {
        ESP_LOGE(TAG, "创建按键任务失败");
        return;
    }

    if (bsp_lvgl_lock(1000)) {
        pc_ui_init();
        bsp_lvgl_unlock();
    }

    const esp_err_t button_err = bsp_button_init(on_key, NULL);
    if (button_err != ESP_OK) {
        ESP_LOGE(TAG, "按键初始化失败: %s;仅长按配网与翻页将不可用",
                 esp_err_to_name(button_err));
    }
    s_input_ready = true;

    if (wifi_setup_prepare() != ESP_OK) {
        ESP_LOGE(TAG, "网络栈初始化失败,本次启动无法联网");
        pc_ui_show_message("网络不可用",
                           "Wi-Fi 初始化失败。请重启设备后重试。");
        return;
    }

    if (xTaskCreate(app_task, "app_ctrl", 6144, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "创建应用任务失败");
    }
}
