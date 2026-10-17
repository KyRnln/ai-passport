// main/pc_ui.c —— 应用自有界面实现(纯黑背景 + 白色元素,横屏 320x240)。
//
// 三键交互:
//   监控主页  确定长按 → 进入设置(短按不动作)
//   设置页    上下 → 选择条目(光标项白色粗边框)
//             确定 → 进入当前条目 / 结束亮度调节
//             长按确定 → 返回监控主页
//   亮度条目  上键调亮、下键调暗(10 档步进,范围 10..100)
//   Wi-Fi 条目 确定 → 二次确认页,确认后才请求进入配网
//   方向条目  确定 → 在两种横屏方向之间翻转(立即生效并持久化)
//   配网态    长按确定退出(由 main 处理,不在本文件)
//
// 字体:assets/fonts/app_font_26.c 与 app_font_13.c,由 tools/gen_cjk_font.py 从
// 汉仪像素 11px U(HYPixel 11px U,--snap 点对点模式)生成的应用子集。
// 该字体的设计网格是 13px/em(77 单位一像素),所以字号必须是 13 的整数倍。
// 设置相关页面与提示页均为中文;顶栏时间、电量与连接状态仍用拉丁字母/数字。改动中文文案后必须重新生成
// 字体,生成脚本会按 main/*.c 的字面量核对字形覆盖,缺字直接报错。
#include "pc_ui.h"

#include "bsp_battery.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "pc_client.h"
#include "wifi_setup.h"

#include "driver/usb_serial_jtag.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "pc_ui";

// 界面字体:26px 正文/数值、13px 说明,都是汉仪像素设计网格(13px/em)的整数倍,
// 且生成时做了二值化,屏幕上每个像素非亮即灭,因此是点对点显示。
LV_FONT_DECLARE(app_font_26);
LV_FONT_DECLARE(app_font_13);
#define UI_FONT_BIG   (&app_font_26)
#define UI_FONT_SMALL (&app_font_13)

// 调色板:纯黑背景 + 白色元素,与设备屏幕的圆角/黑边观感一致。
//   COL_TEXT  纯白 —— 页面正文/数值/进度条填充
//   COL_MUTED 浅灰 —— 顶栏(运行时间与电量)、底部状态行等次要文字
//   COL_DIM   中灰 —— 操作提示
#define COL_BG      0x000000
#define COL_CARD    0x000000
#define COL_FRAME   0x505050   // 卡片/行的 1px 细边框
#define COL_FOCUS   0xFFFFFF   // 光标框:仅指向当前行
#define COL_OK      0x00FF00   // 光标框已确认(按下 OK 进入该项)
#define COL_TRACK   0x303030   // 进度条底槽:深灰
#define COL_TEXT    0xFFFFFF   // 正文/数值,也是三条进度条的填充色
#define COL_MUTED   0xB0B0B0
#define COL_DIM     0x707070
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
#define METRIC_CARD_TOP  35
#define METRIC_TEXT_X    14
#define METRIC_BAR_W     276
#define METRIC_BAR_H     8
#define METRIC_BAR_Y     38
#define ANIM_MS          240

// 顶栏/底部提示行距屏幕上下边缘的留白,以及距左右边缘的留白。
// 顶栏留白按 26px 字体的墨迹高度取值,避免压到第一张指标卡(卡顶 y=38)。
#define TOP_BAR_Y        6
#define BOTTOM_ROW_Y     8
#define EDGE_MARGIN      16

// 顶栏充电闪电:紧贴电量数字左侧。间距、下移量和自身跨度都按屏幕观感微调。
#define TOPBAR_BOLT_GAP  4     // 闪电与电量数字之间的间距
#define TOPBAR_BOLT_DY   7     // 闪电相对顶栏上沿的下移量
#define TOPBAR_BOLT_W    9     // 闪电折线的横向跨度(点坐标 0..9,描边另算)

// 充电指示的闪电折线(约 9x16 的闭合轮廓,描边 2px)。
static const lv_point_precise_t CHARGING_BOLT[] = {
    { 6, 0 }, { 0, 9 }, { 4, 9 }, { 2, 16 }, { 9, 7 }, { 5, 7 }, { 6, 0 },
};

// 纯充电器只送 5V、不枚举数据,因此不会发 SOF,usb_serial_jtag_is_connected() 判不出来;
// ESP32-C3 也没有 VBUS 检测寄存器,板上也没有充电 IC 的状态脚,所以补一条电池电压趋势:
// 每 5 秒采一次 VCELL,窗口内电压持续升高即视为正在充电。代价是插上后约需半分钟才亮,
// 且电量接近满、电压走平后会判为未充电。
#define CHG_MV_SAMPLES      16     // 环形缓冲采样点数(每点约 5 秒,窗口约 80 秒)
#define CHG_MV_MIN_SAMPLES  6      // 至少这么多点(约 30 秒)才判趋势
#define CHG_MV_RISE         8      // 窗口内升/降达到该幅度(mV)才改判
#define CHG_MV_GAP_MS       20000  // 距上次采样超过该时长说明中间没在采,整段作废
#define CHG_FLAT_LIMIT      6      // 连续这么多次采样(约 30 秒)走平 → 视为未充电

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
#define SET_ROW_BORDER_W    2   // 光标框粗细
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
    int target_pct;
    int bar_width;
} metric_row_t;

static lv_obj_t *s_scr;
static page_t s_page = PAGE_NONE;
static lv_timer_t *s_timer;
static lv_obj_t *s_uptime;
static lv_obj_t *s_battery;
static lv_obj_t *s_charging;             // 充电闪电(判定在充电时显示)
static bool s_charging_on;
static int s_cell_mv[CHG_MV_SAMPLES];    // 电池电压采样环形缓冲(充电趋势判据)
static int s_cell_mv_count;
static int s_cell_mv_head;
static int64_t s_cell_mv_ms;             // 上次采样时刻(ms)
static bool s_charge_trend;              // 电压趋势判定:正在充电
static int s_charge_flat;                // 连续走平的窗口数
static lv_obj_t *s_status;
static lv_obj_t *s_cursor_frame;
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
static char s_battery_text[16];

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
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_radius(track, 0, 0);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_remove_flag(track, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *fill = lv_obj_create(track);
    lv_obj_set_pos(fill, 0, 0);
    lv_obj_set_size(fill, 0, h);
    lv_obj_set_style_bg_color(fill, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);   // 填充必须不透明,否则槽色透出会改变观感深浅
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_radius(fill, 0, 0);
    lv_obj_set_style_pad_all(fill, 0, 0);
    lv_obj_remove_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
    return fill;
}

static void animate_bar_width(void *obj, int32_t width)
{
    lv_obj_set_width(obj, width);
}

// 顶栏左侧是 PC 已运行时间(来自 AIDA64),右侧是电量;两处同字号。
// 闪电紧贴电量数字左侧:电量位数会变("100%"→"98%"),标签宽度随之变化,
// 因此按标签实际宽度用像素坐标落位,而不是相对屏幕右边缘固定对齐。
static void place_charging(void)
{
    if (s_charging == NULL || s_battery == NULL) {
        return;
    }
    lv_obj_update_layout(s_battery);
    const int digits_left = BSP_LCD_W - EDGE_MARGIN - lv_obj_get_width(s_battery);
    lv_obj_set_pos(s_charging, digits_left - TOPBAR_BOLT_GAP - TOPBAR_BOLT_W,
                   TOP_BAR_Y + TOPBAR_BOLT_DY);
}

static void build_topbar(lv_obj_t *scr)
{
    s_uptime = make_label(scr, "-", UI_FONT_BIG, COL_MUTED);
    lv_obj_align(s_uptime, LV_ALIGN_TOP_LEFT, EDGE_MARGIN, TOP_BAR_Y);

    s_battery = make_label(scr, "", UI_FONT_BIG, COL_MUTED);
    lv_obj_align(s_battery, LV_ALIGN_TOP_RIGHT, -EDGE_MARGIN, TOP_BAR_Y);

    // 充电闪电:折线画在电量数字左侧,默认隐藏,USB 连接时显示。
    s_charging = lv_line_create(scr);
    lv_line_set_points(s_charging, CHARGING_BOLT,
                       sizeof(CHARGING_BOLT) / sizeof(CHARGING_BOLT[0]));
    lv_obj_set_style_line_width(s_charging, 2, 0);
    lv_obj_set_style_line_color(s_charging, lv_color_hex(COL_OK), 0);
    lv_obj_add_flag(s_charging, LV_OBJ_FLAG_HIDDEN);
    place_charging();
}

// 底部一行:左侧固定显示连接状态(常显),右侧显示当前页面的按键提示。
static void build_bottom_row(lv_obj_t *scr, const char *right_text)
{
    s_status = make_label(scr, "", UI_FONT_SMALL, COL_MUTED);
    lv_obj_align(s_status, LV_ALIGN_BOTTOM_LEFT, EDGE_MARGIN, -BOTTOM_ROW_Y);

    lv_obj_t *right = make_label(scr, right_text, UI_FONT_SMALL, COL_DIM);
    lv_obj_align(right, LV_ALIGN_BOTTOM_RIGHT, -EDGE_MARGIN, -BOTTOM_ROW_Y);
}

// 释放当前页面。必须在持有 LVGL 锁时调用。
static void clear_screen(void)
{
    for (int i = 0; i < METRIC_COUNT; ++i) {
        if (s_rows[i].fill != NULL) {
            lv_anim_delete(s_rows[i].fill, animate_bar_width);
        }
    }
    if (s_scr != NULL) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
    for (int i = 0; i < METRIC_COUNT; ++i) {
        s_rows[i].value = NULL;
        s_rows[i].fill = NULL;
        s_rows[i].target_pct = -1;
        s_rows[i].bar_width = 0;
        s_row_full[i] = false;
    }
    for (int i = 0; i < SET_ROW_COUNT; ++i) {
        s_set_value[i] = NULL;
    }
    s_uptime = NULL;
    s_battery = NULL;
    s_charging = NULL;
    s_charging_on = false;
    s_status = NULL;
    s_cursor_frame = NULL;
    s_uptime_text[0] = '\0';
    s_battery_text[0] = '\0';
    s_page = PAGE_NONE;
}

// 设置列表第 index 行的 y 坐标。
static int set_row_y(int index)
{
    return SET_ROW_TOP + index * (SET_ROW_H + SET_ROW_GAP);
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
        lv_obj_set_style_border_color(card, lv_color_hex(COL_FRAME), 0);
        lv_obj_set_style_radius(card, 0, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        // 垂直卡片:上行"名称 + 数值",下行整宽进度条。
        lv_obj_t *name = make_label(card, names[i], UI_FONT_BIG, COL_TEXT);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, METRIC_TEXT_X, 4);

        s_rows[i].value = make_label(card, "-", UI_FONT_BIG, COL_TEXT);
        lv_obj_align(s_rows[i].value, LV_ALIGN_TOP_RIGHT, -METRIC_TEXT_X, 4);
        s_rows[i].target_pct = -1;
        s_rows[i].bar_width = 0;

        s_rows[i].fill = make_bar(card, METRIC_TEXT_X, METRIC_BAR_Y, METRIC_BAR_W,
                                  METRIC_BAR_H, COL_TEXT);
    }

    // 左侧常显连接状态,右侧是进入设置的按键提示。
    build_bottom_row(s_scr, "长按 OK: 进入设置");

    lv_screen_load(s_scr);
}

// 光标框颜色:只表示当前所在行时为白色,按下 OK 进入该项(如调亮度)后为绿色。
static uint32_t cursor_color(void)
{
    return s_bright_edit ? COL_OK : COL_FOCUS;
}

static void build_settings(void)
{
    static const char *const names[SET_ROW_COUNT] = { "亮度", "Wi-Fi",
                                                      "方向", "设备信息" };

    s_scr = make_screen();
    build_topbar(s_scr);

    for (int i = 0; i < SET_ROW_COUNT; ++i) {
        // 选项行本身不画框,当前行与"已进入"状态统一由下面的光标框表示。
        lv_obj_t *row = lv_obj_create(s_scr);
        lv_obj_set_pos(row, SET_ROW_X, set_row_y(i));
        lv_obj_set_size(row, SET_ROW_W, SET_ROW_H);
        lv_obj_set_style_bg_color(row, lv_color_hex(COL_CARD), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = make_label(row, names[i], UI_FONT_BIG, COL_TEXT);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, SET_ROW_TEXT_X, 0);

        s_set_value[i] = make_label(row, "", UI_FONT_BIG, COL_TEXT);
        lv_obj_align(s_set_value[i], LV_ALIGN_RIGHT_MID, -SET_ROW_TEXT_X, 0);
    }

    // 光标框:与当前行等高,有且只有这一个框;默认白色,进入后绿色。
    s_cursor_frame = lv_obj_create(s_scr);
    lv_obj_set_pos(s_cursor_frame, SET_ROW_X, set_row_y(s_set_index));
    lv_obj_set_size(s_cursor_frame, SET_ROW_W, SET_ROW_H);
    lv_obj_set_style_bg_opa(s_cursor_frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_cursor_frame, SET_ROW_BORDER_W, 0);
    lv_obj_set_style_border_color(s_cursor_frame, lv_color_hex(cursor_color()), 0);
    lv_obj_set_style_radius(s_cursor_frame, 0, 0);
    lv_obj_set_style_pad_all(s_cursor_frame, 0, 0);
    lv_obj_remove_flag(s_cursor_frame, LV_OBJ_FLAG_SCROLLABLE);

    // 左侧常显连接状态,右侧是当前条目的操作提示。
    build_bottom_row(s_scr, s_bright_edit ? "OK: 完成" : "长按 OK: 返回");

    lv_screen_load(s_scr);
}

static void build_info(void)
{
    s_scr = make_screen();
    build_topbar(s_scr);

    char target[96];
    if (s_has_cfg) {
        snprintf(target, sizeof(target), "%s:%u", s_cfg.host, (unsigned)s_cfg.port);
    } else {
        snprintf(target, sizeof(target), "未配置");
    }

    lv_obj_t *head = make_label(s_scr, "AIDA64", UI_FONT_BIG, COL_TEXT);
    lv_obj_align(head, LV_ALIGN_TOP_LEFT, 10, 44);

    lv_obj_t *t = make_label(s_scr, target, UI_FONT_BIG, COL_TEXT);
    lv_obj_set_width(t, 300);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);   // 地址过长时用省略号,避免出屏
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 10, 78);

    lv_obj_t *src = make_label(s_scr, "数据源  AIDA64 RemoteSensor /sse",
                               UI_FONT_SMALL, COL_TEXT);
    lv_obj_align(src, LV_ALIGN_TOP_LEFT, 10, 116);

    lv_obj_t *note = make_label(s_scr,
                                "在 AIDA64 中启用 RemoteSensor,\n"
                                "添加这些 LCD 项:\n"
                                "CPU 使用率 / GPU 使用率 /\n"
                                "内存使用率 / 运行时间",
                                UI_FONT_SMALL, COL_MUTED);
    lv_obj_set_width(note, 300);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_align(note, LV_ALIGN_TOP_LEFT, 10, 136);

    build_bottom_row(s_scr, "OK: 返回");

    lv_screen_load(s_scr);
}

static void build_confirm(void)
{
    s_scr = make_screen();
    build_topbar(s_scr);

    lv_obj_t *question = make_label(s_scr, "开始 Wi-Fi 配网?", UI_FONT_BIG,
                                    COL_TEXT);
    lv_obj_set_width(question, 300);
    lv_label_set_long_mode(question, LV_LABEL_LONG_WRAP);
    lv_obj_align(question, LV_ALIGN_TOP_LEFT, 10, 56);

    lv_obj_t *note = make_label(s_scr,
                                "监控会暂停, 设备将开启配网热点。",
                                UI_FONT_SMALL, COL_MUTED);
    lv_obj_set_width(note, 300);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_align(note, LV_ALIGN_TOP_LEFT, 10, 108);

    build_bottom_row(s_scr, "OK: 开始  UP/DOWN: 取消");

    lv_screen_load(s_scr);
}

static void build_setup(void)
{
    s_scr = make_screen();
    build_topbar(s_scr);

    lv_obj_t *body = make_label(s_scr,
                                "连接 Wi-Fi:\n"
                                WIFI_SETUP_AP_SSID "\n"
                                "密码: " WIFI_SETUP_AP_PASS "\n"
                                "打开: " WIFI_SETUP_AP_IP,
                                UI_FONT_BIG, COL_TEXT);
    lv_obj_set_width(body, 300);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 10, 40);

    lv_obj_t *note = make_label(s_scr,
                                "手机扫码选择 Wi-Fi, 再填入 PC 地址。",
                                UI_FONT_SMALL, COL_MUTED);
    lv_obj_set_width(note, 300);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_align(note, LV_ALIGN_TOP_LEFT, 10, 160);

    // 配网态左侧同样常显连接状态(此时通常为 No Wi-Fi)。
    build_bottom_row(s_scr, "长按 OK: 退出配网");

    lv_screen_load(s_scr);
}

static void build_message(const char *title, const char *body_text)
{
    s_scr = make_screen();
    build_topbar(s_scr);

    lv_obj_t *title_label = make_label(s_scr, title, UI_FONT_BIG, COL_TEXT);
    lv_obj_set_width(title_label, 300);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 10, 56);

    lv_obj_t *body_label = make_label(s_scr, body_text, UI_FONT_SMALL, COL_MUTED);
    lv_obj_set_width(body_label, 300);
    lv_label_set_long_mode(body_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(body_label, LV_ALIGN_TOP_LEFT, 10, 108);

    // 提示页的文案由 main 提供(启动与配网流程)。
    build_bottom_row(s_scr, "OK: 返回");

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
// 仅在原始时长含天数时显示 "xxD "。文本变化才重设,避免定时刷新反复重绘。
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
        pc_uptime_format(metrics.uptime_sec, metrics.uptime_has_days, text, sizeof(text));
    }

    if (strcmp(text, s_uptime_text) != 0) {
        snprintf(s_uptime_text, sizeof(s_uptime_text), "%s", text);
        lv_label_set_text(s_uptime, text);
    }
}

// 把一次 VCELL 采样喂进环形缓冲并更新趋势判定。
// 最新点比窗口最老点高 CHG_MV_RISE 以上 → 充电中;低同样幅度 → 未充电;
// 走平先沿用原判定,连续 CHG_FLAT_LIMIT 次采样都走平才判为未充电:
// 这样满电后电压走平时闪电不会立刻熄灭,而拔掉充电器后电压会立刻下跌,很快熄灭。
static void charge_trend_update(int mv)
{
    const int64_t now_ms = esp_timer_get_time() / 1000;
    if (mv < 0) {
        return;                             // 读不到电压:不采样也不改判定
    }
    if (s_cell_mv_ms != 0 && now_ms - s_cell_mv_ms > CHG_MV_GAP_MS) {
        s_cell_mv_count = 0;                // 中间长时间没采,旧数据不再代表当前趋势
        s_cell_mv_head = 0;
        s_charge_flat = 0;
    }
    s_cell_mv_ms = now_ms;

    s_cell_mv[s_cell_mv_head] = mv;
    s_cell_mv_head = (s_cell_mv_head + 1) % CHG_MV_SAMPLES;
    if (s_cell_mv_count < CHG_MV_SAMPLES) {
        s_cell_mv_count++;
    }
    if (s_cell_mv_count < CHG_MV_MIN_SAMPLES) {
        return;                             // 采样点还不够,先不下结论
    }

    const int oldest = s_cell_mv[(s_cell_mv_head + CHG_MV_SAMPLES - s_cell_mv_count) % CHG_MV_SAMPLES];
    const int delta = mv - oldest;
    if (delta >= CHG_MV_RISE) {
        s_charge_trend = true;
        s_charge_flat = 0;
    } else if (delta <= -CHG_MV_RISE) {
        s_charge_trend = false;
        s_charge_flat = 0;
    } else if (++s_charge_flat >= CHG_FLAT_LIMIT) {
        s_charge_trend = false;
    }
}

// 充电指示:插在电脑等带数据的 USB 口上(主机在发 SOF)立即显示;
// 插纯充电器时没有 SOF,改由电池电压趋势判定(见 charge_trend_update)。
static void refresh_charging(void)
{
    if (s_charging == NULL) {
        return;
    }
    const bool on = usb_serial_jtag_is_connected() || s_charge_trend;
    if (on == s_charging_on) {
        return;
    }
    s_charging_on = on;
    if (on) {
        lv_obj_remove_flag(s_charging, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_charging, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_battery(void)
{
    if (s_battery == NULL) {
        return;
    }
    char text[16];
    const int soc = bsp_battery_soc();
    charge_trend_update(bsp_battery_mv());
    if (soc < 0) {
        text[0] = '\0';                     // 读不到就不画数字
    } else {
        snprintf(text, sizeof(text), "%d%%", soc);
    }
    if (strcmp(text, s_battery_text) != 0) {
        snprintf(s_battery_text, sizeof(s_battery_text), "%s", text);
        lv_label_set_text(s_battery, text);
        place_charging();   // 文字宽度变了,闪电跟着落到新位置
    }
}

static void update_metric_value(metric_row_t *row, int pct)
{
    if (pct == row->target_pct) {
        return;
    }
    row->target_pct = pct;
    lv_label_set_text_fmt(row->value, "%d%%", pct);
}

// 进度条宽度缓动;数值本身直接替换,不做滚动。
static void update_metric_bar(metric_row_t *row, int width)
{
    if (width == row->bar_width) {
        return;
    }
    row->bar_width = width;
    const int current = lv_obj_get_width(row->fill);
    lv_anim_delete(row->fill, animate_bar_width);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, row->fill);
    lv_anim_set_exec_cb(&anim, animate_bar_width);
    lv_anim_set_values(&anim, current, width);
    lv_anim_set_duration(&anim, ANIM_MS);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
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
            if (s_rows[i].target_pct != -1) {
                s_rows[i].target_pct = -1;
                s_rows[i].bar_width = 0;
                lv_label_set_text(s_rows[i].value, "-");
                lv_anim_delete(s_rows[i].fill, animate_bar_width);
                lv_obj_set_width(s_rows[i].fill, 0);
            }
            if (s_row_full[i]) {
                s_row_full[i] = false;
                lv_obj_set_style_bg_color(s_rows[i].fill,
                                          lv_color_hex(COL_TEXT), 0);
            }
            continue;
        }

        const float value = values[i] < 0.0f ? 0.0f : values[i];
        const float clamped = value > 100.0f ? 100.0f : value;
        const bool full = clamped >= 100.0f;

        update_metric_value(&s_rows[i], (int)(clamped + 0.5f));
        update_metric_bar(&s_rows[i],
                          (int)(METRIC_BAR_W * clamped / 100.0f + 0.5f));

        // 满格转为告警红;仅在状态变化时改样式,避免每帧都触发重绘。
        if (full != s_row_full[i]) {
            s_row_full[i] = full;
            lv_obj_set_style_bg_color(s_rows[i].fill,
                                      lv_color_hex(full ? COL_FULL : COL_TEXT), 0);
        }
    }
}

static void refresh_status(void)
{
    if (s_status == NULL) {
        return;
    }

    // 底部左侧常显连接状态,四种文案覆盖所有情况:
    //   没有 Wi-Fi                        → "无 Wi-Fi"
    //   数据正常(2 秒内收到新数据)        → "已连接"
    //   连不上 PC(客户端正在重试)         → "PC 离线"
    //   其余(连接中、已连上但断流)        → "连接中"
    // 文案与右侧按键提示同行,必须短。
    if (!wifi_setup_sta_has_ip()) {
        lv_label_set_text(s_status, "无 Wi-Fi");
        return;
    }

    pc_client_state_t state = PC_CLIENT_IDLE;
    int64_t last_update_ms = 0;
    pc_client_snapshot(NULL, &state, &last_update_ms);

    if (state == PC_CLIENT_STREAMING && !metrics_are_stale(last_update_ms)) {
        lv_label_set_text(s_status, "已连接");
    } else if (state == PC_CLIENT_RETRYING) {
        lv_label_set_text(s_status, "PC 离线");
    } else {
        lv_label_set_text(s_status, "连接中");
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
                          wifi_setup_sta_has_ip() ? "已连接" : "未连接");
    }
    if (s_set_value[SET_ROW_ROTATION] != NULL) {
        lv_label_set_text(s_set_value[SET_ROW_ROTATION],
                          s_flipped ? "已翻转" : "正常");
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
        refresh_charging();
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
    refresh_charging();
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
    refresh_charging();
    refresh_status();
    bsp_lvgl_unlock();
}

// 把光标框放到当前选中行(直接落位,不做滑动动画)。
static void place_cursor(void)
{
    if (s_cursor_frame == NULL) {
        return;
    }
    lv_obj_set_pos(s_cursor_frame, SET_ROW_X, set_row_y(s_set_index));
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
            // 列表里"上"= 选中项索引减一;光标框直接落到新行,不重建整页。
            const int delta = is_up ? -1 : 1;
            s_set_index = (s_set_index + delta + SET_ROW_COUNT) % SET_ROW_COUNT;
            place_cursor();
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
