# main —— AI Passport PC Monitor 应用(派生应用)。
#
# 基线的 demo_*.c / ui_pixel*.c 仍保留在仓库中作为参考实现与主机测试的编译输入,
# 但不再编入本应用固件:本应用的界面与流程完全由 pc_* 模块提供。
idf_component_register(
    SRCS "main.c"
         "pc_metrics.c"
         "pc_config.c"
         "http_form.c"
         "wifi_setup.c"
         "pc_client.c"
         "pc_ui.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_driver_usb_serial_jtag esp_event esp_http_client esp_http_server esp_netif esp_timer esp_wifi nvs_flash
)

# 界面字体:由 tools/gen_cjk_font.py 从缝合像素字体
# (Fusion Pixel Font 12px Monospaced zh-Hans, SIL OFL 1.1,见 assets/README.md)
# 生成的 LVGL 子集。改动中文文案后需重新生成这两个文件。
# assets/fonts/proggy_*.c 是旧的像素字体,保留作参考但不再编入本应用固件。
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/app_font_24.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/app_font_12.c"
)
