// Host-test stub for the single esp_lcd panel operation used by
// components/bsp/src/bsp_display_lvgl.c (bsp_lvgl_set_flipped).
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_lcd_types.h"

static inline esp_err_t esp_lcd_panel_mirror(esp_lcd_panel_handle_t panel,
                                             bool mirror_x, bool mirror_y) {
    (void)panel;
    (void)mirror_x;
    (void)mirror_y;
    return ESP_OK;
}
