// main/pc_ui.c —— 应用自有界面实现(纯黑终端 + 绿色文字,横屏 320x240)。
//
// 三键交互:
//   监控主页  确定长按 → 进入设置(短按不动作)
//   设置页    上下 → 选择条目(光标项细边框)
//             确定 → 进入当前条目 / 结束亮度调节
//             长按确定 → 返回监控主页
//   亮度条目  上键调亮、下键调暗(10 档步进,范围 10..100)
//   Wi-Fi 条目 确定 → 二次确认页,确认后才请求进入配网
//   旋转条目  确定 → 在两种横屏方向之间翻转(立即生效并持久化)
//   配网态    长按确定退出(由 main 处理,不在本文件)
//
// 约定:所有 LVGL 文案只用 ASCII。界面字体为 ProggyClean(仅 ASCII 字形,
// assets/fonts/proggy_*.c),写中文只会显示占位方框;中文字体属于单独任务。
#include "pc_ui.h"

#include "bsp_battery.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "pc_client.h"
#include "wifi_setup.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "pc_ui";

// ProggyClean 是 16px 原生像素字体,16 的整数倍渲染才保持清晰;
// 因此只用 16(提示/说明)与 32(标题/数值/条目)两档。
LV_FONT_DECLARE(proggy_16);
LV_FONT_DECLARE(proggy_32);

// 调色板:纯黑背景 + 绿色文字,取自经典终端配色。
//   COL_TEXT  最亮 —— 页面正文/数值/条目
//   COL_MUTED 中绿 —— 顶栏(运行时间与电量)、底部状态行等次要文字
//   COL_DIM   最暗 —— 操作提示
#define COL_BG      0x000000
#define COL_CARD    0x000000
#define COL_CURSOR  0x00B400   // 光标项:浅绿细边框
#define COL_SELECT  0x00FF00   // 已进入项:亮绿粗边框
#define COL_TRACK   0x003C00
#define COL_TEXT    0x00FF00
#define COL_MUTED   0x00B400
#define COL_DIM     0x007800
#define COL_CPU     0x00FF00
#define COL_GPU     0x00E600
#define COL_MEM     0x00CC00
#define COL_FULL    0xFF2020   // 进度满格时的告警红

// ---------------------------------------------------------------------------
// 布局(横屏 320x240)
// ---------------------------------------------------------------------------
// 监控主页:三条指标卡片上下堆叠(垂直布局),在顶栏与底部提示行之间均分。
#define METRIC_COUNT     3
#define METRIC_CPU       0
#define METRIC_GPU       1
#define METRIC_MEM       2
#define METRIC_CARD_X    8
#define METRIC_CARD_W    304
#define METRIC_CARD_H    56
#define METRIC_CARD_GAP  5
#define METRIC_CARD_TOP  38
#define METRIC_TEXT_X    14
#define METRIC_BAR_W     276
#define METRIC_BAR_H     8
#define METRIC_BAR_Y     41

// 顶栏/底部提示行距屏幕上下边缘的留白,以及距左右边缘的留白。
#define TOP_BAR_Y        10
#define BOTTOM_ROW_Y     8
#define EDGE_MARGIN      16

static const uint32_t METRIC_ACCENT[METRIC_COUNT] = { COL_CPU, COL_GPU, COL_MEM };

// 无新数据超过该时长,数值显示 "-"。
#define METRICS_STALE_MS 2000

// 设置页条目:亮度 / Wi-Fi 配网 / 横屏方向 / 设备信息。
#define SET_ROW_COUNT       4
#define SET_ROW_BRIGHTNESS  0
#define SET_ROW_WIFI        1
#define SET_ROW_ROTATION    2
#define SET_ROW_INFO        3
#define SET_ROW_X           8
#define SET_ROW_W           304
#define SET_ROW_H           36
#define SET_ROW_GAP         8
#define SET_ROW_TOP         41
#define SET_ROW_BORDER_W    2
#define SET_ROW_TEXT_X      10

typedef enum {
    PAGE_NONE = 0,
    PAGE_MONITOR,
    PAGE_SETTINGS,
    PAGE_INFO,
    PAGE_CONFIRM,
    PAGE_SETUP,
    PAGE_MESSAGE,
} page_t;

typedef struct {
    lv_obj_t *value;
    lv_obj_t *fill;
} metric_row_t;

static lv_obj_t *s_scr;
static page_t s_page = PAGE_NONE;
static lv_timer_t *s_timer;
static lv_obj_t *s_uptime;
static lv_obj_t *s_battery;
static lv_obj_t *s_status;
static metric_row_t s_rows[METRIC_COUNT];
static bool s_row_full[METRIC_COUNT];   // 该条进度是否已满格(满格显示红色)
static lv_obj_t *s_set_value[SET_ROW_COUNT];
static int s_set_index;
static bool s_bright_edit;
static uint8_t s_brightness = PC_CFG_BRIGHTNESS_DEFAULT;
static bool s_flipped;
static pc_config_t s_cfg;
static bool s_has_cfg;
static uint32_t s_tick;
static char s_uptime_text[16];

// ---------------------------------------------------------------------------
// 通用构件
// ---------------------------------------------------------------------------
static lv_obj_t *make_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

// 进度条:返回填充条对象(宽度由调用方按比例设置)。
static lv_obj_t *make_bar(lv_obj_t *parent, int x, int y, int w, int h,
                          uint32_t color)
{
    lv_obj_t *track = lv_obj_create(parent);
    lv_obj_set_pos(track, x, y);
    lv_obj_set_size(track, w, h);
    lv_obj_set_style_bg_color(track, lv_color_hex(COL_TRACK), 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_radius(track, 0, 0);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *fill = lv_obj_create(track);
    lv_obj_set_pos(fill, 0, 0);
    lv_obj_set_size(fill, 0, h);
    lv_obj_set_style_bg_color(fill, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_radius(fill, 0, 0);
    lv_obj_set_style_pad_all(fill, 0, 0);
    lv_obj_remove_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
    return fill;
}

// 顶栏左侧是 PC 已运行时间(来自 AIDA64),右侧是电量;两处同字号同高。
static void build_topbar(lv_obj_t *scr)
{
    s_uptime = make_label(scr, "-", &proggy_32, COL_MUTED);
    lv_obj_align(s_uptime, LV_ALIGN_TOP_LEFT, EDGE_MARGIN, TOP_BAR_Y);

    s_battery = make_label(scr, "", &proggy_32, COL_MUTED);
    lv_obj_align(s_battery, LV_ALIGN_TOP_RIGHT, -EDGE_MARGIN, TOP_BAR_Y);
}

// 底部一行:左侧固定显示连接状态(常显),右侧显示当前页面的按键提示。
static void build_bottom_row(lv_obj_t *scr, const char *right_text)
{
    s_status = make_label(scr, "", &proggy_16, COL_MUTED);
    lv_obj_align(s_status, LV_ALIGN_BOTTOM_LEFT, EDGE_MARGIN, -BOTTOM_ROW_Y);

    lv_obj_t *right = make_label(scr, right_text, &proggy_16, COL_DIM);
    lv_obj_align(right, LV_ALIGN_BOTTOM_RIGHT, -EDGE_MARGIN, -BOTTOM_ROW_Y);
}

// 释放当前页面。必须在持有 LVGL 锁时调用。
static void clear_screen(void)
{
    if (s_scr != NULL) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
    for (int i = 0; i < METRIC_COUNT; ++i) {
        s_rows[i].value = NULL;
        s_rows[i].fill = NULL;
        s_row_full[i] = false;
    }
    for (int i = 0; i < SET_ROW_COUNT; ++i) {
        s_set_value[i] = NULL;
    }
    s_uptime = NULL;
    s_battery = NULL;
    s_status = NULL;
    s_uptime_text[0] = '\0';
    s_page = PAGE_NONE;
}

// ---------------------------------------------------------------------------
// 页面构建
// ---------------------------------------------------------------------------
static void build_monitor(void)
{
    static const char *const names[METRIC_COUNT] = { "CPU", "GPU", "MEM" };

    s_scr = make_screen();
    build_topbar(s_scr);

    for (int i = 0; i < METRIC_COUNT; ++i) {
        const int y = METRIC_CARD_TOP + i * (METRIC_CARD_H + METRIC_CARD_GAP);

        lv_obj_t *card = lv_obj_create(s_scr);
        lv_obj_set_pos(card, METRIC_CARD_X, y);
        lv_obj_set_size(card, METRIC_CARD_W, METRIC_CARD_H);
        lv_obj_set_style_bg_color(card, lv_color_hex(COL_CARD), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x005500), 0);
        lv_obj_set_style_radius(card, 0, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        // 垂直卡片:上行"名称 + 数值",下行整宽进度条。
        lv_obj_t *name = make_label(card, names[i], &proggy_32, COL_TEXT);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, METRIC_TEXT_X, 7);

        s_rows[i].value = make_label(card, "-", &proggy_32, COL_TEXT);
        lv_obj_align(s_rows[i].value, LV_ALIGN_TOP_RIGHT, -METRIC_TEXT_X, 7);

        s_rows[i].fill = make_bar(card, METRIC_TEXT_X, METRIC_BAR_Y, METRIC_BAR_W,
                                  METRIC_BAR_H, METRIC_ACCENT[i]);
    }

    // 左侧常显连接状态,右侧是进入设置的按键提示。
    build_bottom_row(s_scr, "Hold OK: settings");

    lv_screen_load(s_scr);
}

static void build_settings(void)
{
    static const char *const names[SET_ROW_COUNT] = { "Brightness", "Wi-Fi",
                                                      "Rotation", "Device Info" };

    s_scr = make_screen();
    build_topbar(s_scr);

    for (int i = 0; i < SET_ROW_COUNT; ++i) {
        // 三种状态:
        //   已进入(亮度调节中) → 2px 亮绿粗边框
        //   上下光标所在项       → 1px 浅绿细边框
        //   其余未选择项         → 无边框
        const bool entered = s_bright_edit && (i == SET_ROW_BRIGHTNESS);
        const bool cursor = (i == s_set_index) && !entered;

        int border_w = 0;
        uint32_t border_color = COL_CURSOR;
        if (entered) {
            border_w = SET_ROW_BORDER_W;
            border_color = COL_SELECT;
        } else if (cursor) {
            border_w = 1;
            border_color = COL_CURSOR;
        }

        const int y = SET_ROW_TOP + i * (SET_ROW_H + SET_ROW_GAP);

        lv_obj_t *row = lv_obj_create(s_scr);
        lv_obj_set_pos(row, SET_ROW_X, y);
        lv_obj_set_size(row, SET_ROW_W, SET_ROW_H);
        lv_obj_set_style_bg_color(row, lv_color_hex(COL_CARD), 0);
        lv_obj_set_style_border_width(row, border_w, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(border_color), 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = make_label(row, names[i], &proggy_32, COL_TEXT);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, SET_ROW_TEXT_X, 0);

        s_set_value[i] = make_label(row, "", &proggy_32, COL_TEXT);
        lv_obj_align(s_set_value[i], LV_ALIGN_RIGHT_MID, -SET_ROW_TEXT_X, 0);
    }

    // 左侧常显连接状态,右侧是当前条目的操作提示。
    build_bottom_row(s_scr, s_bright_edit ? "OK: done" : "Hold OK: back");

    lv_screen_load(s_scr);
}

static void build_info(void)
{
    s_scr = make_screen();
    build_topbar(s_scr);

    char target[96];
    if (s_has_cfg) {
        snprintf(target, sizeof(target), "AIDA64  %s:%u", s_cfg.host,
                 (unsigned)s_cfg.port);
    } else {
        snprintf(target, sizeof(target), "AIDA64  not configured");
    }

    lv_obj_t *t = make_label(s_scr, target, &proggy_16, COL_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 10, 44);

    lv_obj_t *src = make_label(s_scr, "Source  AIDA64 RemoteSensor /sse",
                               &proggy_16, COL_TEXT);
    lv_obj_align(src, LV_ALIGN_TOP_LEFT, 10, 66);

    lv_obj_t *note = make_label(s_scr,
                                "Enable RemoteSensor in AIDA64 and add simple sensor\n"
                                "items named:  CPU Utilization / GPU Utilization /\n"
                                "Memory Utilization / UpTime",
                                &proggy_16, COL_MUTED);
    lv_obj_set_width(note, 300);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_align(note, LV_ALIGN_TOP_LEFT, 10, 94);

    build_bottom_row(s_scr, "OK: back");

    lv_screen_load(s_scr);
}

static void build_confirm(void)
{
    s_scr = make_screen();
    build_topbar(s_scr);

    lv_obj_t *question = make_label(s_scr, "Start Wi-Fi setup?", &proggy_32,
                                    COL_TEXT);
    lv_obj_set_width(question, 300);
    lv_label_set_long_mode(question, LV_LABEL_LONG_WRAP);
    lv_obj_align(question, LV_ALIGN_TOP_LEFT, 10, 56);

    lv_obj_t *note = make_label(s_scr,
                                "Monitoring pauses and the device starts a setup hotspot.",
                                &proggy_16, COL_MUTED);
    lv_obj_set_width(note, 300);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_align(note, LV_ALIGN_TOP_LEFT, 10, 108);

    build_bottom_row(s_scr, "OK: yes  UP/DOWN: no");

    lv_screen_load(s_scr);
}

static void build_setup(void)
{
    s_scr = make_screen();
    build_topbar(s_scr);

    lv_obj_t *body = make_label(s_scr,
                                "Join Wi-Fi:\n"
                                WIFI_SETUP_AP_SSID "\n"
                                "pw: " WIFI_SETUP_AP_PASS "\n"
                                "Open: " WIFI_SETUP_AP_IP,
                                &proggy_32, COL_TEXT);
    lv_obj_set_width(body, 300);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 10, 44);

    lv_obj_t *note = make_label(s_scr,
                                "Scan and pick your Wi-Fi, then enter the PC IP.",
                                &proggy_16, COL_MUTED);
    lv_obj_set_width(note, 300);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_align(note, LV_ALIGN_TOP_LEFT, 10, 158);

    // 配网态左侧同样常显连接状态(此时通常为 No Wi-Fi)。
    build_bottom_row(s_scr, "Hold OK: leave setup");

    lv_screen_load(s_scr);
}

static void build_message(const char *title, const char *body_text)
{
    s_scr = make_screen();
    build_topbar(s_scr);

    lv_obj_t *title_label = make_label(s_scr, title, &proggy_32, COL_TEXT);
    lv_obj_set_width(title_label, 300);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 10, 56);

    lv_obj_t *body_label = make_label(s_scr, body_text, &proggy_16, COL_MUTED);
    lv_obj_set_width(body_label, 300);
    lv_label_set_long_mode(body_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(body_label, LV_ALIGN_TOP_LEFT, 10, 108);

    build_bottom_row(s_scr, "OK: back");

    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------------------
// 周期刷新(LVGL 定时器 → 在 LVGL 任务里执行,无需额外加锁)
// ---------------------------------------------------------------------------
// 本地刷新判定:超过 METRICS_STALE_MS 没有新数据(或从未收到)即视为失效。
static bool metrics_are_stale(int64_t last_update_ms)
{
    if (last_update_ms <= 0) {
        return true;
    }
    return (esp_timer_get_time() / 1000 - last_update_ms) > METRICS_STALE_MS;
}

// 顶栏运行时间:读取 AIDA64 返回的 UpTime(PC 已运行时间),不是本机开机时间。
static void refresh_uptime(void)
{
    if (s_uptime == NULL) {
        return;
    }

    pc_metrics_t metrics;
    int64_t last_update_ms = 0;
    pc_client_snapshot(&metrics, NULL, &last_update_ms);

    char text[16];
    if (metrics_are_stale(last_update_ms) || !metrics.uptime_valid) {
        snprintf(text, sizeof(text), "-");
    } else {
        const uint32_t total = metrics.uptime_sec;
        snprintf(text, sizeof(text), "%u:%02u:%02u", (unsigned)(total / 3600u),
                 (unsigned)((total / 60u) % 60u), (unsigned)(total % 60u));
    }

    // 文本没变就不重设,避免定时刷新反复触发重绘。
    if (strcmp(text, s_uptime_text) != 0) {
        memcpy(s_uptime_text, text, sizeof(text));
        lv_label_set_text(s_uptime, text);
    }
}

static void refresh_battery(void)
{
    if (s_battery == NULL) {
        return;
    }
    const int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_label_set_text(s_battery, "");  // 读不到就不画数字
    } else {
        lv_label_set_text_fmt(s_battery, "%d%%", soc);
    }
}

static void refresh_metrics(void)
{
    pc_metrics_t metrics;
    int64_t last_update_ms = 0;
    pc_client_snapshot(&metrics, NULL, &last_update_ms);

    // 本地刷新:超过 2 秒没有新数据,一律显示 "-",避免把旧值当实时值。
    const bool stale = metrics_are_stale(last_update_ms);

    const float values[METRIC_COUNT] = { metrics.cpu_pct, metrics.gpu_pct,
                                         metrics.mem_pct };
    const bool valid[METRIC_COUNT] = { metrics.cpu_valid, metrics.gpu_valid,
                                       metrics.mem_valid };

    for (int i = 0; i < METRIC_COUNT; ++i) {
        if (s_rows[i].value == NULL || s_rows[i].fill == NULL) {
            continue;
        }
        if (stale || !valid[i]) {
            lv_label_set_text(s_rows[i].value, "-");
            lv_obj_set_width(s_rows[i].fill, 0);
            if (s_row_full[i]) {
                s_row_full[i] = false;
                lv_obj_set_style_bg_color(s_rows[i].fill,
                                          lv_color_hex(METRIC_ACCENT[i]), 0);
            }
            continue;
        }

        const float value = values[i] < 0.0f ? 0.0f : values[i];
        const float clamped = value > 100.0f ? 100.0f : value;
        const bool full = clamped >= 100.0f;

        lv_label_set_text_fmt(s_rows[i].value, "%d%%", (int)(clamped + 0.5f));
        lv_obj_set_width(s_rows[i].fill,
                         (int32_t)(METRIC_BAR_W * clamped / 100.0f + 0.5f));

        // 满格转为告警红;仅在状态变化时改样式,避免每帧都触发重绘。
        if (full != s_row_full[i]) {
            s_row_full[i] = full;
            lv_obj_set_style_bg_color(s_rows[i].fill,
                                      lv_color_hex(full ? COL_FULL
                                                        : METRIC_ACCENT[i]), 0);
        }
    }
}

static void refresh_status(void)
{
    if (s_status == NULL) {
        return;
    }

    // 底部左侧常显连接状态,四种文案覆盖所有情况:
    //   没有 Wi-Fi                        → "No Wi-Fi"
    //   数据正常(2 秒内收到新数据)        → "Connected"
    //   连不上 PC(客户端正在重试)         → "PC offline"
    //   其余(连接中、已连上但断流)        → "Connecting"
    // 文案与右侧按键提示同行,必须短。
    if (!wifi_setup_sta_has_ip()) {
        lv_label_set_text(s_status, "No Wi-Fi");
        return;
    }

    pc_client_state_t state = PC_CLIENT_IDLE;
    int64_t last_update_ms = 0;
    pc_client_snapshot(NULL, &state, &last_update_ms);

    if (state == PC_CLIENT_STREAMING && !metrics_are_stale(last_update_ms)) {
        lv_label_set_text(s_status, "Connected");
    } else if (state == PC_CLIENT_RETRYING) {
        lv_label_set_text(s_status, "PC offline");
    } else {
        lv_label_set_text(s_status, "Connecting");
    }
}

static void refresh_settings(void)
{
    if (s_set_value[SET_ROW_BRIGHTNESS] != NULL) {
        lv_label_set_text_fmt(s_set_value[SET_ROW_BRIGHTNESS], "%u%%",
                              (unsigned)s_brightness);
    }
    if (s_set_value[SET_ROW_WIFI] != NULL) {
        lv_label_set_text(s_set_value[SET_ROW_WIFI],
                          wifi_setup_sta_has_ip() ? "Connected" : "Offline");
    }
    if (s_set_value[SET_ROW_ROTATION] != NULL) {
        lv_label_set_text(s_set_value[SET_ROW_ROTATION],
                          s_flipped ? "Flipped" : "Normal");
    }
    if (s_set_value[SET_ROW_INFO] != NULL) {
        lv_label_set_text(s_set_value[SET_ROW_INFO], ">");
    }
}

// 应用并持久化亮度。必须持有 LVGL 锁。
static void apply_brightness(uint8_t percent)
{
    s_brightness = percent;
    bsp_display_backlight(s_brightness);
    if (pc_config_save_brightness(s_brightness) != ESP_OK) {
        // 保存失败只影响下次开机,当前亮度仍然生效。
        ESP_LOGW(TAG, "亮度保存失败,重启后恢复上次记录");
    }
    refresh_settings();
}

// 翻转横屏方向并持久化。必须持有 LVGL 锁。
static void apply_flipped(bool flipped)
{
    if (bsp_lvgl_set_flipped(flipped) != ESP_OK) {
        ESP_LOGW(TAG, "横屏方向设置失败");
        return;
    }
    s_flipped = flipped;
    if (pc_config_save_flipped(s_flipped) != ESP_OK) {
        ESP_LOGW(TAG, "横屏方向保存失败,重启后恢复上次记录");
    }
}

static void ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_scr == NULL) {
        return;
    }

    s_tick++;
    if (s_tick % 16 == 0) {
        refresh_battery();  // 约 5 秒一次,避免频繁 I2C 读打断刷屏
    }
    if (s_tick % 4 == 0) {
        refresh_uptime();   // 约 1 秒一次
    }

    if (s_page == PAGE_MONITOR) {
        refresh_metrics();
    } else if (s_page == PAGE_SETTINGS) {
        refresh_settings();
    }
    refresh_status();
}

// 释放当前页并构建目标页(必须持有 LVGL 锁)。
static void load_page(page_t page)
{
    clear_screen();
    switch (page) {
    case PAGE_MONITOR:
        build_monitor();
        break;
    case PAGE_SETTINGS:
        build_settings();
        break;
    case PAGE_INFO:
        build_info();
        break;
    case PAGE_CONFIRM:
        build_confirm();
        break;
    case PAGE_SETUP:
        build_setup();
        break;
    default:
        break;
    }
    s_page = page;

    refresh_uptime();
    refresh_battery();
    refresh_status();
    if (page == PAGE_MONITOR) {
        refresh_metrics();
    } else if (page == PAGE_SETTINGS) {
        refresh_settings();
    }
}

// ---------------------------------------------------------------------------
// 对外接口(自行加锁)
// ---------------------------------------------------------------------------
void pc_ui_init(void)
{
    // 亮度与横屏方向都在 NVS 初始化之后加载并立即生效。
    uint8_t saved = PC_CFG_BRIGHTNESS_DEFAULT;
    (void)pc_config_load_brightness(&saved);
    s_brightness = saved;
    bsp_display_backlight(s_brightness);

    bool flipped = false;
    (void)pc_config_load_flipped(&flipped);
    (void)bsp_lvgl_set_flipped(flipped);
    s_flipped = flipped;

    if (s_timer == NULL) {
        s_timer = lv_timer_create(ui_timer_cb, 300, NULL);
    }
}

void pc_ui_show_monitor(void)
{
    if (!bsp_lvgl_lock(500)) {
        return;
    }
    load_page(PAGE_MONITOR);
    bsp_lvgl_unlock();
}

void pc_ui_show_setup(void)
{
    if (!bsp_lvgl_lock(500)) {
        return;
    }
    load_page(PAGE_SETUP);
    bsp_lvgl_unlock();
}

void pc_ui_show_message(const char *title, const char *body)
{
    if (title == NULL || body == NULL) {
        return;
    }
    if (!bsp_lvgl_lock(500)) {
        return;
    }
    clear_screen();
    build_message(title, body);
    s_page = PAGE_MESSAGE;
    refresh_uptime();
    refresh_battery();
    refresh_status();
    bsp_lvgl_unlock();
}

static pc_ui_action_t handle_arrow(bool is_up, bool is_long)
{
    (void)is_long;   // 亮度固定 10 档步进,长按不再加速

    if (!bsp_lvgl_lock(500)) {
        return PC_UI_ACTION_NONE;
    }

    switch (s_page) {
    case PAGE_SETTINGS:
        if (s_bright_edit) {
            // 上键调亮、下键调暗,固定 10 档步进,范围 10..100。
            int next = (int)s_brightness +
                       (is_up ? PC_CFG_BRIGHTNESS_STEP : -PC_CFG_BRIGHTNESS_STEP);
            if (next < PC_CFG_BRIGHTNESS_MIN) {
                next = PC_CFG_BRIGHTNESS_MIN;
            }
            if (next > PC_CFG_BRIGHTNESS_MAX) {
                next = PC_CFG_BRIGHTNESS_MAX;
            }
            apply_brightness((uint8_t)next);
        } else {
            // 列表里"上"= 选中项索引减一。
            const int delta = is_up ? -1 : 1;
            s_set_index = (s_set_index + delta + SET_ROW_COUNT) % SET_ROW_COUNT;
            load_page(PAGE_SETTINGS);
        }
        break;
    case PAGE_INFO:
    case PAGE_CONFIRM:
        // 上下键等同"返回设置页"/"取消"。
        load_page(PAGE_SETTINGS);
        break;
    default:
        break;
    }

    bsp_lvgl_unlock();
    return PC_UI_ACTION_NONE;
}

pc_ui_action_t pc_ui_on_up(bool is_long)
{
    return handle_arrow(true, is_long);
}

pc_ui_action_t pc_ui_on_down(bool is_long)
{
    return handle_arrow(false, is_long);
}

pc_ui_action_t pc_ui_on_ok(bool is_long)
{
    if (!bsp_lvgl_lock(500)) {
        return PC_UI_ACTION_NONE;
    }

    pc_ui_action_t action = PC_UI_ACTION_NONE;

    switch (s_page) {
    case PAGE_MONITOR:
        // 只有长按确定才进入设置;短按不动作。
        if (is_long) {
            s_set_index = SET_ROW_BRIGHTNESS;
            s_bright_edit = false;
            load_page(PAGE_SETTINGS);
        }
        break;
    case PAGE_SETTINGS:
        if (is_long) {
            load_page(PAGE_MONITOR);            // 长按返回监控主页
        } else if (s_bright_edit) {
            s_bright_edit = false;
            load_page(PAGE_SETTINGS);
        } else if (s_set_index == SET_ROW_BRIGHTNESS) {
            s_bright_edit = true;
            load_page(PAGE_SETTINGS);
        } else if (s_set_index == SET_ROW_WIFI) {
            load_page(PAGE_CONFIRM);
        } else if (s_set_index == SET_ROW_ROTATION) {
            apply_flipped(!s_flipped);
            load_page(PAGE_SETTINGS);           // 重建以刷新取值显示
        } else {
            load_page(PAGE_INFO);
        }
        break;
    case PAGE_INFO:
        load_page(PAGE_SETTINGS);
        break;
    case PAGE_CONFIRM:
        if (is_long) {
            load_page(PAGE_SETTINGS);           // 长按视为取消
        } else {
            action = PC_UI_ACTION_ENTER_SETUP;  // 由 main 真正进入配网
        }
        break;
    case PAGE_MESSAGE:
        load_page(PAGE_MONITOR);
        break;
    default:
        break;
    }

    bsp_lvgl_unlock();
    return action;
}

void pc_ui_set_config(const pc_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    if (!bsp_lvgl_lock(500)) {
        return;
    }
    s_cfg = *cfg;
    s_has_cfg = true;
    bsp_lvgl_unlock();
}
