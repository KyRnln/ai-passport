// main/pc_config.c —— NVS 持久化实现。
#include "pc_config.h"

#include "esp_log.h"
#include "nvs.h"

#include <string.h>

static const char *TAG = "pc_config";
static const char *NVS_NAMESPACE = "pccfg";
static const char *KEY_SSID = "ssid";
static const char *KEY_PASS = "pass";
static const char *KEY_HOST = "host";
static const char *KEY_PORT = "port";

// 本机 UI 设置放独立命名空间:pc_config_clear() 只清网络配置,不动这些值。
static const char *NVS_UI_NAMESPACE = "pcui";
static const char *KEY_BRIGHTNESS = "bright";
static const char *KEY_FLIPPED = "flip";

void pc_config_set_defaults(pc_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    memset(cfg, 0, sizeof(*cfg));
    cfg->port = PC_CFG_PORT_DEFAULT;
}

bool pc_config_complete(const pc_config_t *cfg)
{
    return cfg != NULL && cfg->ssid[0] != '\0' && cfg->host[0] != '\0';
}

static esp_err_t read_str(nvs_handle_t handle, const char *key, char *out, size_t out_size)
{
    size_t len = out_size;
    const esp_err_t err = nvs_get_str(handle, key, out, &len);
    if (err != ESP_OK) {
        return err;
    }
    // nvs_get_str 会写入 NUL;len==0 属于异常记录。
    return len == 0 ? ESP_ERR_INVALID_SIZE : ESP_OK;
}

esp_err_t pc_config_load(pc_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    pc_config_set_defaults(cfg);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开 NVS 命名空间失败: %s", esp_err_to_name(err));
        return err;
    }

    err = read_str(handle, KEY_SSID, cfg->ssid, sizeof(cfg->ssid));
    if (err != ESP_OK) {
        nvs_close(handle);
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NOT_FOUND : err;
    }

    err = read_str(handle, KEY_HOST, cfg->host, sizeof(cfg->host));
    if (err != ESP_OK) {
        nvs_close(handle);
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NOT_FOUND : err;
    }

    // 密码允许缺失/为空(开放网络);端口缺失则回落默认值。
    (void)read_str(handle, KEY_PASS, cfg->password, sizeof(cfg->password));
    uint16_t port = PC_CFG_PORT_DEFAULT;
    if (nvs_get_u16(handle, KEY_PORT, &port) == ESP_OK && port != 0) {
        cfg->port = port;
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t pc_config_save(const pc_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开 NVS 写句柄失败: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, KEY_SSID, cfg->ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, KEY_PASS, cfg->password);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, KEY_HOST, cfg->host);
    }
    if (err == ESP_OK) {
        err = nvs_set_u16(handle, KEY_PORT, cfg->port == 0 ? PC_CFG_PORT_DEFAULT : cfg->port);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存配置失败: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t pc_config_clear(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t pc_config_load_brightness(uint8_t *percent)
{
    if (percent == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *percent = PC_CFG_BRIGHTNESS_DEFAULT;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_UI_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t value = PC_CFG_BRIGHTNESS_DEFAULT;
    err = nvs_get_u8(handle, KEY_BRIGHTNESS, &value);
    if (err == ESP_OK) {
        if (value > PC_CFG_BRIGHTNESS_MAX) {
            value = PC_CFG_BRIGHTNESS_MAX;
        }
        // 对齐到 10% 档位:旧版本按 1% 步进保存的值(如 57%)也回到整档。
        value = (uint8_t)(((value + PC_CFG_BRIGHTNESS_STEP / 2) /
                           PC_CFG_BRIGHTNESS_STEP) * PC_CFG_BRIGHTNESS_STEP);
        if (value < PC_CFG_BRIGHTNESS_MIN) {
            value = PC_CFG_BRIGHTNESS_MIN;
        }
        *percent = value;
    }
    nvs_close(handle);
    return err;
}

esp_err_t pc_config_save_brightness(uint8_t percent)
{
    if (percent < PC_CFG_BRIGHTNESS_MIN) {
        percent = PC_CFG_BRIGHTNESS_MIN;
    } else if (percent > PC_CFG_BRIGHTNESS_MAX) {
        percent = PC_CFG_BRIGHTNESS_MAX;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_UI_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开亮度设置句柄失败: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_u8(handle, KEY_BRIGHTNESS, percent);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t pc_config_load_flipped(bool *flipped)
{
    if (flipped == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *flipped = false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_UI_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t value = 0;
    err = nvs_get_u8(handle, KEY_FLIPPED, &value);
    if (err == ESP_OK) {
        *flipped = (value != 0);
    }
    nvs_close(handle);
    return err;
}

esp_err_t pc_config_save_flipped(bool flipped)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_UI_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "打开横屏方向设置句柄失败: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_set_u8(handle, KEY_FLIPPED, flipped ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}
