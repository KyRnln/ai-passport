// main/pc_ui.h —— 本应用自有界面(深色仪表盘风格)。
//
// 这是派生应用自己的 UI,不复用基线 demo 的测试菜单 / ui_pixel 视觉外壳。
// 所有函数内部自行获取 bsp_lvgl_lock(),调用方不得已经持有该锁。
#pragma once

#include "pc_config.h"

// LVGL 就绪后调用一次:创建绘制用样式与刷新定时器(不创建页面)。
void pc_ui_init(void);

// 显示监控主页 / 配网引导页 / 通用提示页。
void pc_ui_show_monitor(void);
void pc_ui_show_setup(void);
void pc_ui_show_message(const char *title, const char *body);

// 上/下键:在监控主页与设备信息页之间切换。
void pc_ui_next_page(void);

// 供信息页展示当前目标 PC。
void pc_ui_set_config(const pc_config_t *cfg);
