// main/pc_ui.h —— 本应用自有界面(横屏 320x240,纯黑背景 + 白色元素)。
//
// 这是派生应用自己的 UI,不复用基线 demo 的测试菜单 / ui_pixel 视觉外壳。
// 所有函数内部自行获取 bsp_lvgl_lock(),调用方不得已经持有该锁。
//
// 三键交互:
//   监控主页  确定长按 → 进入设置(短按不动作)
//   设置页    上下 → 选择条目(光标项细边框);确定 → 进入/确认;长按确定 → 返回主页
//   亮度条目  上键调亮、下键调暗(10 档步进,范围 10..100);确定 → 结束调节
//   旋转条目  确定 → 在两种横屏方向间翻转(立即生效并持久化)
//   Wi-Fi 条目 确定 → 二次确认页,确认才请求进入配网
//   配网态    长按确定退出(由 main 处理)
//
// 顶栏左侧显示 PC 已运行时间(仅原始值含天数时显示 xD),右侧显示电量。
// 底部左侧常显连接状态(No Wi-Fi / Connected / Connecting / PC offline,永不空白),
// 右侧是当前页面的按键提示。
#pragma once

#include "pc_config.h"

#include <stdbool.h>

// 界面请求的应用级动作;main 据此决定是否切换运行模式。
typedef enum {
    PC_UI_ACTION_NONE = 0,
    PC_UI_ACTION_ENTER_SETUP,   // 用户已在二次确认页确认进入 Wi-Fi 配网
} pc_ui_action_t;

// LVGL 就绪后调用一次:应用已保存的背光亮度,并创建刷新定时器(不创建页面)。
// 必须在 nvs_flash_init() 之后调用。
void pc_ui_init(void);

// 显示监控主页 / 配网引导页 / 通用提示页。
void pc_ui_show_monitor(void);
void pc_ui_show_setup(void);
void pc_ui_show_message(const char *title, const char *body);

// 按键事件(短按/长按)。返回界面请求的应用级动作。
pc_ui_action_t pc_ui_on_up(bool is_long);
pc_ui_action_t pc_ui_on_down(bool is_long);
pc_ui_action_t pc_ui_on_ok(bool is_long);

// 供设备信息页展示当前目标 PC。
void pc_ui_set_config(const pc_config_t *cfg);
