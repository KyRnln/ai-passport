// main/pc_ui.c —— 应用自有界面实现(深色仪表盘)。
//
// 约定:所有 LVGL 文案只用 ASCII。默认 Montserrat 字体没有中文字形,
// 直接写中文会显示成方框;中文字体集成属于单独任务,本版本不引入。
#include "pc_ui.h"

#include "bsp_battery.h"
#include "bsp_display.h"
#include "pc_client.h"
#include "wifi_setup.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

// 调色板:刻意区别于基线 demo 的浅色像素风格。
#define COL_BG     0x0E1420
#define COL_CARD   0x18202F
#define COL_TRACK  0x2A3550
#define COL_TEXT   0xE8EEF7
#define COL_MUTED  0x8EA0B8
#define COL_DIM    0x6F8098
#define COL_CPU    0x4CC2FF
#define COL_GPU    0x9B7BFF
#define COL_MEM    0x39D98A

#define METRIC_COUNT 3
#define BAR_WIDTH 188
#define BAR_HEIGHT 8

typedef enum {
    PAGE_NONE = 0,
    PAGE_MONITOR,
    PAGE_INFO,
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
static lv_obj_t *s_battery;
static lv_obj_t *s_status;
static metric_row_t s_rows[METRIC_COUNT];
static pc_config_t s_cfg;
static bool s_has_cfg;
static uint32_t s_tick;

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

static void build_topbar(lv_obj_t *scr, const char *title)
{
    lv_obj_t *label = make_label(scr, title, &lv_font_montserrat_20, COL_TEXT);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 14, 12);

    s_battery = make_label(scr, "", &lv_font_montserrat_14, COL_MUTED);
    lv_obj_align(s_battery, LV_ALIGN_TOP_RIGHT, -14, 18);
}

static void build_status_line(lv_obj_t *scr)
{
    s_status = make_label(scr, "", &lv_font_montserrat_14, COL_MUTED);
    lv_obj_align(s_status, LV_ALIGN_BOTTOM_LEFT, 14, -30);
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
    }
    s_battery = NULL;
    s_status = NULL;
    s_page = PAGE_NONE;
}

// ---------------------------------------------------------------------------
// 页面构建
// ---------------------------------------------------------------------------
static void build_monitor(void)
{
    static const char *const titles[METRIC_COUNT] = { "CPU", "GPU", "MEM" };
    static const uint32_t accents[METRIC_COUNT] = { COL_CPU, COL_GPU, COL_MEM };

    s_scr = make_screen();
    build_topbar(s_scr, "PC MONITOR");

    for (int i = 0; i < METRIC_COUNT; ++i) {
        const int y = 46 + i * 68;

        lv_obj_t *card = lv_obj_create(s_scr);
        lv_obj_set_pos(card, 12, y);
        lv_obj_set_size(card, 216, 60);
        lv_obj_set_style_bg_color(card, lv_color_hex(COL_CARD), 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_radius(card, 14, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = make_label(card, titles[i], &lv_font_montserrat_14, COL_MUTED);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 14, 8);

        s_rows[i].value = make_label(card, "--", &lv_font_montserrat_28, COL_TEXT);
        lv_obj_align(s_rows[i].value, LV_ALIGN_TOP_RIGHT, -14, 4);

        lv_obj_t *track = lv_obj_create(card);
        lv_obj_set_pos(track, 14, 42);
        lv_obj_set_size(track, BAR_WIDTH, BAR_HEIGHT);
        lv_obj_set_style_bg_color(track, lv_color_hex(COL_TRACK), 0);
        lv_obj_set_style_border_width(track, 0, 0);
        lv_obj_set_style_radius(track, BAR_HEIGHT / 2, 0);
        lv_obj_set_style_pad_all(track, 0, 0);
        lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);

        s_rows[i].fill = lv_obj_create(track);
        lv_obj_set_pos(s_rows[i].fill, 0, 0);
        lv_obj_set_size(s_rows[i].fill, 0, BAR_HEIGHT);
        lv_obj_set_style_bg_color(s_rows[i].fill, lv_color_hex(accents[i]), 0);
        lv_obj_set_style_border_width(s_rows[i].fill, 0, 0);
        lv_obj_set_style_radius(s_rows[i].fill, BAR_HEIGHT / 2, 0);
        lv_obj_set_style_pad_all(s_rows[i].fill, 0, 0);
        lv_obj_remove_flag(s_rows[i].fill, LV_OBJ_FLAG_SCROLLABLE);
    }

    build_status_line(s_scr);

    lv_obj_t *hint = make_label(s_scr, "UP/DOWN: pages   Hold OK: setup",
                                &lv_font_montserrat_14, COL_DIM);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 14, -10);

    lv_screen_load(s_scr);
}

static void build_info(void)
{
    s_scr = make_screen();
    build_topbar(s_scr, "DEVICE INFO");

    char target[96];
    if (s_has_cfg) {
        snprintf(target, sizeof(target), "AIDA64  %s:%u", s_cfg.host,
                 (unsigned)s_cfg.port);
    } else {
        snprintf(target, sizeof(target), "AIDA64  not configured");
    }

    lv_obj_t *t = make_label(s_scr, target, &lv_font_montserrat_14, COL_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 54);

    lv_obj_t *src = make_label(s_scr, "Source  AIDA64 RemoteSensor /sse",
                               &lv_font_montserrat_14, COL_MUTED);
    lv_obj_align(src, LV_ALIGN_TOP_LEFT, 14, 82);

    lv_obj_t *note = make_label(s_scr,
                                "Enable RemoteSensor in AIDA64 and add\n"
                                "Simple sensor items named:\n"
                                "  CPU Utilization\n"
                                "  GPU Utilization\n"
                                "  Memory Utilization",
                                &lv_font_montserrat_14, COL_DIM);
    lv_obj_align(note, LV_ALIGN_TOP_LEFT, 14, 116);

    build_status_line(s_scr);

    lv_obj_t *hint = make_label(s_scr, "UP/DOWN: pages   Hold OK: setup",
                                &lv_font_montserrat_14, COL_DIM);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 14, -10);

    lv_screen_load(s_scr);
}

static void build_setup(void)
{
    s_scr = make_screen();
    build_topbar(s_scr, "WI-FI SETUP");

    lv_obj_t *body = make_label(s_scr,
                                "1. On your phone, join Wi-Fi:\n"
                                "     " WIFI_SETUP_AP_SSID "\n"
                                "     password: " WIFI_SETUP_AP_PASS "\n"
                                "2. Open " WIFI_SETUP_AP_URL "\n"
                                "3. Enter your 2.4GHz Wi-Fi and the\n"
                                "     PC address running AIDA64.\n\n"
                                "The device verifies the Wi-Fi and\n"
                                "starts monitoring automatically.",
                                &lv_font_montserrat_14, COL_TEXT);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 14, 52);

    build_status_line(s_scr);

    lv_obj_t *hint = make_label(s_scr, "Hold OK: leave setup", &lv_font_montserrat_14,
                                COL_DIM);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 14, -10);

    lv_screen_load(s_scr);
}

static void build_message(const char *title, const char *body_text)
{
    s_scr = make_screen();
    build_topbar(s_scr, "PC MONITOR");

    lv_obj_t *title_label = make_label(s_scr, title, &lv_font_montserrat_20, COL_TEXT);
    lv_obj_set_width(title_label, 208);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 14, 60);

    lv_obj_t *body_label = make_label(s_scr, body_text, &lv_font_montserrat_14, COL_MUTED);
    lv_obj_set_width(body_label, 208);
    lv_label_set_long_mode(body_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(body_label, LV_ALIGN_TOP_LEFT, 14, 108);

    build_status_line(s_scr);

    lv_obj_t *hint = make_label(s_scr, "Hold OK: setup", &lv_font_montserrat_14, COL_DIM);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 14, -10);

    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------------------
// 周期刷新(LVGL 定时器 → 在 LVGL 任务里执行,无需额外加锁)
// ---------------------------------------------------------------------------
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
    pc_client_snapshot(&metrics, NULL, NULL);

    const float values[METRIC_COUNT] = { metrics.cpu_pct, metrics.gpu_pct,
                                         metrics.mem_pct };
    const bool valid[METRIC_COUNT] = { metrics.cpu_valid, metrics.gpu_valid,
                                       metrics.mem_valid };

    for (int i = 0; i < METRIC_COUNT; ++i) {
        if (s_rows[i].value == NULL || s_rows[i].fill == NULL) {
            continue;
        }
        if (valid[i]) {
            const float clamped = values[i] < 0.0f ? 0.0f
                                : (values[i] > 100.0f ? 100.0f : values[i]);
            lv_label_set_text_fmt(s_rows[i].value, "%d%%", (int)(clamped + 0.5f));
            lv_obj_set_width(s_rows[i].fill,
                             (int32_t)(BAR_WIDTH * clamped / 100.0f + 0.5f));
        } else {
            lv_label_set_text(s_rows[i].value, "--");
            lv_obj_set_width(s_rows[i].fill, 0);
        }
    }
}

static void refresh_status(void)
{
    if (s_status == NULL) {
        return;
    }

    if (!wifi_setup_sta_has_ip()) {
        lv_label_set_text(s_status, "Waiting for Wi-Fi...");
        return;
    }

    pc_client_state_t state = PC_CLIENT_IDLE;
    pc_client_snapshot(NULL, &state, NULL);

    switch (state) {
    case PC_CLIENT_STREAMING:
        lv_label_set_text(s_status, "Streaming from PC");
        break;
    case PC_CLIENT_CONNECTING:
        lv_label_set_text(s_status, "Connecting to PC...");
        break;
    case PC_CLIENT_RETRYING:
        lv_label_set_text(s_status, "PC offline - retrying");
        break;
    default:
        lv_label_set_text(s_status, "Idle");
        break;
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

    if (s_page == PAGE_MONITOR) {
        refresh_metrics();
    }
    refresh_status();
}

// ---------------------------------------------------------------------------
// 对外接口(自行加锁)
// ---------------------------------------------------------------------------
void pc_ui_init(void)
{
    if (s_timer == NULL) {
        s_timer = lv_timer_create(ui_timer_cb, 300, NULL);
    }
}

void pc_ui_show_monitor(void)
{
    if (!bsp_lvgl_lock(500)) {
        return;
    }
    clear_screen();
    build_monitor();
    s_page = PAGE_MONITOR;
    refresh_battery();
    refresh_status();
    bsp_lvgl_unlock();
}

void pc_ui_show_setup(void)
{
    if (!bsp_lvgl_lock(500)) {
        return;
    }
    clear_screen();
    build_setup();
    s_page = PAGE_SETUP;
    refresh_battery();
    refresh_status();
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
    refresh_battery();
    refresh_status();
    bsp_lvgl_unlock();
}

void pc_ui_next_page(void)
{
    if (!bsp_lvgl_lock(500)) {
        return;
    }
    const page_t current = s_page;
    clear_screen();
    if (current == PAGE_MONITOR) {
        build_info();
        s_page = PAGE_INFO;
    } else {
        build_monitor();
        s_page = PAGE_MONITOR;
    }
    refresh_battery();
    refresh_status();
    bsp_lvgl_unlock();
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
