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
    REQUIRES bsp esp_event esp_http_client esp_http_server esp_netif esp_timer esp_wifi nvs_flash
)
