// main/wifi_setup.c —— Wi-Fi STA 连接 + SoftAP 网页配网实现。
#include "wifi_setup.h"

#include "http_form.h"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <inttypes.h>
#include <string.h>

static const char *TAG = "wifi_setup";

#define BIT_STA_GOT_IP BIT0

// 表单字段上限。字段长度上限在 pc_config.h 中定义,这里只做整体 body 上限。
#define FORM_BODY_MAX 512
#define FORM_RECV_RETRY_MAX 8

static EventGroupHandle_t s_events;
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static httpd_handle_t s_httpd;
static bool s_wifi_ready;
static bool s_ap_active;
static volatile bool s_sta_want_connect;
static volatile bool s_configured;

// ---------------------------------------------------------------------------
// 事件处理
// ---------------------------------------------------------------------------
static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)data;

    switch (id) {
    case WIFI_EVENT_STA_START:
        if (s_sta_want_connect) {
            (void)esp_wifi_connect();
        }
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        xEventGroupClearBits(s_events, BIT_STA_GOT_IP);
        if (s_sta_want_connect) {
            ESP_LOGW(TAG, "STA 断开,重试连接");
            (void)esp_wifi_connect();
        }
        break;
    case WIFI_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "手机已连接到配网热点");
        break;
    case WIFI_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "手机已断开配网热点");
        break;
    default:
        break;
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;

    if (id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "STA 已获取 IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_events, BIT_STA_GOT_IP);
    }
}

// ---------------------------------------------------------------------------
// 网络栈初始化
// ---------------------------------------------------------------------------
esp_err_t wifi_setup_prepare(void)
{
    if (s_wifi_ready) {
        return ESP_OK;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init 失败: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "创建默认事件循环失败: %s", esp_err_to_name(err));
        return err;
    }

    s_events = xEventGroupCreate();
    if (s_events == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // 与 ESP-IDF 官方示例一致:先建默认 netif,再初始化 Wi-Fi 驱动。
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (s_sta_netif == NULL || s_ap_netif == NULL) {
        ESP_LOGE(TAG, "创建默认网络接口失败");
        return ESP_ERR_NO_MEM;
    }

    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init 失败: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &on_wifi_event, NULL, NULL);
    if (err == ESP_OK) {
        err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                  &on_ip_event, NULL, NULL);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "注册网络事件处理失败: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "启动 Wi-Fi 失败: %s", esp_err_to_name(err));
        return err;
    }

    s_wifi_ready = true;
    ESP_LOGI(TAG, "网络栈就绪");
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// STA
// ---------------------------------------------------------------------------
esp_err_t wifi_setup_sta_connect(const pc_config_t *cfg)
{
    if (!s_wifi_ready || cfg == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    wifi_config_t wifi_cfg = {0};
    strlcpy((char *)wifi_cfg.sta.ssid, cfg->ssid, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, cfg->password, sizeof(wifi_cfg.sta.password));

    xEventGroupClearBits(s_events, BIT_STA_GOT_IP);

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "写入 STA 配置失败: %s", esp_err_to_name(err));
        return err;
    }

    s_sta_want_connect = true;
    err = esp_wifi_connect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
        ESP_LOGE(TAG, "发起 STA 连接失败: %s", esp_err_to_name(err));
        s_sta_want_connect = false;
        return err;
    }

    ESP_LOGI(TAG, "正在连接 Wi-Fi: %s", cfg->ssid);
    return ESP_OK;
}

void wifi_setup_sta_disconnect(void)
{
    s_sta_want_connect = false;
    if (s_events != NULL) {
        xEventGroupClearBits(s_events, BIT_STA_GOT_IP);
    }
    if (s_wifi_ready) {
        (void)esp_wifi_disconnect();
    }
}

bool wifi_setup_sta_wait_connected(uint32_t timeout_ms)
{
    if (s_events == NULL) {
        return false;
    }

    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    for (;;) {
        const TickType_t now = xTaskGetTickCount();
        if (now >= deadline) {
            return false;
        }
        const EventBits_t bits = xEventGroupWaitBits(s_events, BIT_STA_GOT_IP,
                                                     pdFALSE, pdFALSE, deadline - now);
        if ((bits & BIT_STA_GOT_IP) != 0) {
            return true;
        }
    }
}

bool wifi_setup_sta_has_ip(void)
{
    return s_events != NULL && (xEventGroupGetBits(s_events) & BIT_STA_GOT_IP) != 0;
}

// ---------------------------------------------------------------------------
// 配网网页
// ---------------------------------------------------------------------------
static const char PAGE_HTML[] =
    "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>AI Passport 配网</title><style>"
    "body{font-family:system-ui,-apple-system,sans-serif;margin:0;padding:24px;"
    "background:#0f1420;color:#e8eef7}"
    "h1{font-size:20px;margin:0 0 4px}p{color:#8ea0b8;font-size:13px;margin:0 0 18px}"
    "label{display:block;font-size:13px;margin:14px 0 6px;color:#a9bcd4}"
    "input{width:100%;box-sizing:border-box;padding:11px;border-radius:10px;"
    "border:1px solid #2a3550;background:#161d2e;color:#e8eef7;font-size:15px}"
    "button{margin-top:22px;width:100%;padding:13px;border:0;border-radius:10px;"
    "background:#2f7cf6;color:#fff;font-size:16px;font-weight:600}"
    "#scanBtn{width:auto;margin:10px 0 0;padding:9px 14px;font-size:13px;"
    "background:#26324a;color:#cfe0ff;font-weight:500}"
    "#scanBtn:disabled{opacity:.6}"
    ".ap{margin-top:8px;padding:10px 12px;border-radius:8px;background:#161d2e;"
    "border:1px solid #2a3550;font-size:14px;color:#cfe0ff;cursor:pointer;"
    "display:flex;justify-content:space-between;align-items:center;gap:10px}"
    ".ap.sel{border-color:#2f7cf6;background:#1b2740}"
    ".ap small{color:#6f8098;font-size:12px;white-space:nowrap}"
    "#list{margin-top:6px;font-size:13px;color:#6f8098;line-height:1.5}"
    ".hint{margin-top:16px;font-size:12px;color:#6f8098;line-height:1.5}"
    "</style></head><body>"
    "<h1>AI Passport</h1>"
    "<p>连接 2.4GHz Wi-Fi,并填入运行 AIDA64 的电脑地址</p>"
    "<form method=\"post\" action=\"/save\">"
    "<label>Wi-Fi 名称 (SSID)</label>"
    "<input name=\"ssid\" id=\"ssid\" maxlength=\"32\" required>"
    "<button type=\"button\" id=\"scanBtn\">扫描附近的 Wi-Fi</button>"
    "<div id=\"list\"></div>"
    "<label>Wi-Fi 密码</label><input name=\"pass\" type=\"password\" maxlength=\"63\">"
    "<label>AIDA64 电脑地址</label><input name=\"host\" maxlength=\"63\" "
    "placeholder=\"192.168.1.10\" required>"
    "<label>AIDA64 端口</label><input name=\"port\" maxlength=\"5\" value=\"80\">"
    "<button type=\"submit\">保存并连接</button></form>"
    "<div class=\"hint\">端口需与 AIDA64 RemoteSensor 设置一致(默认 80)。"
    "保存后设备会先验证 Wi-Fi 连接,成功才写入配置并开始监控。</div>"
    "<script>"
    "var apList=document.getElementById('list');"
    "function pick(ssid,el){document.getElementById('ssid').value=ssid;"
    "var a=document.querySelectorAll('.ap');for(var i=0;i<a.length;i++)"
    "{a[i].classList.remove('sel');}el.classList.add('sel');}"
    "function scanDone(){var b=document.getElementById('scanBtn');b.disabled=false;"
    "b.textContent='重新扫描';}"
    "function scan(){var b=document.getElementById('scanBtn');b.disabled=true;"
    "b.textContent='扫描中...';apList.textContent='正在扫描,约需数秒...';tryScan(1);}"
    "function tryScan(left){"
    "fetch('/scan').then(function(r){return r.json();}).then(function(list){"
    "apList.textContent='';"
    "if(!list.length){apList.textContent='没扫到 2.4GHz 网络,可手动填写 SSID。';scanDone();return;}"
    "list.forEach(function(x){var d=document.createElement('div');d.className='ap';"
    "var n=document.createElement('span');n.textContent=x.ssid;d.appendChild(n);"
    "var m=document.createElement('small');"
    "m.textContent=x.rssi+' dBm'+(x.secure?'':' | 开放');d.appendChild(m);"
    "d.onclick=function(){pick(x.ssid,d);};apList.appendChild(d);});scanDone();})"
    ".catch(function(){if(left>0){apList.textContent='扫描中断,2 秒后重试...';"
    "setTimeout(function(){tryScan(left-1);},2000);return;}"
    "apList.textContent='扫描失败,请重试或手动填写 SSID。';scanDone();});}"
    // 在脚本里绑定,避免内联 onclick 的 form 作用域把 scan 解析成同名按钮。
    "document.getElementById('scanBtn').onclick=scan;"
    "</script>"
    "</body></html>";

// 分块发送一个简短结果页,避免为此再多留一块页面缓冲。
static esp_err_t send_result(httpd_req_t *req, const char *status, const char *title,
                            const char *message)
{
    static const char HEAD[] =
        "<!DOCTYPE html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<style>body{font-family:system-ui,-apple-system,sans-serif;margin:0;padding:28px;"
        "background:#0f1420;color:#e8eef7}h1{font-size:19px}p{color:#a9bcd4;font-size:14px;"
        "line-height:1.6}</style></head><body><h1>";
    static const char MID[] = "</h1><p>";
    static const char TAIL[] = "</p></body></html>";

    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send_chunk(req, HEAD, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, title, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, MID, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, message, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, TAIL, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t handler_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    // 配置页必须每次取新的:否则手机浏览器可能一直用缓存的旧页面。
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, PAGE_HTML, HTTPD_RESP_USE_STRLEN);
}

// 有界的 JSON 拼接:SSID 是外部输入,必须转义,且整体不能溢出。
typedef struct {
    char *buf;
    size_t cap;
    size_t len;
} json_writer_t;

static void jw_put(json_writer_t *w, const char *text)
{
    while (*text != '\0' && w->len + 1 < w->cap) {
        w->buf[w->len++] = *text++;
    }
    w->buf[w->len] = '\0';
}

static void jw_put_escaped(json_writer_t *w, const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; ++p) {
        char piece[8];
        if (*p == '"' || *p == '\\') {
            piece[0] = '\\';
            piece[1] = (char)*p;
            piece[2] = '\0';
        } else if (*p < 0x20) {
            snprintf(piece, sizeof(piece), "\\u%04x", (unsigned)*p);
        } else {
            piece[0] = (char)*p;   // >=0x80 的 UTF-8 字节原样输出
            piece[1] = '\0';
        }
        jw_put(w, piece);
    }
}

static esp_err_t handler_scan(httpd_req_t *req)
{
    static wifi_setup_ap_t aps[WIFI_SETUP_SCAN_MAX];
    static char json[2048];

    const int count = wifi_setup_scan(aps, WIFI_SETUP_SCAN_MAX);
    if (count < 0) {
        ESP_LOGW(TAG, "/scan 失败");
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        return httpd_resp_send(req, "[]", HTTPD_RESP_USE_STRLEN);
    }

    json_writer_t w = { .buf = json, .cap = sizeof(json), .len = 0 };
    json[0] = '\0';
    jw_put(&w, "[");
    for (int i = 0; i < count; ++i) {
        char tail[48];
        if (i > 0) {
            jw_put(&w, ",");
        }
        jw_put(&w, "{\"ssid\":\"");
        jw_put_escaped(&w, aps[i].ssid);
        snprintf(tail, sizeof(tail), "\",\"rssi\":%d,\"secure\":%s}", (int)aps[i].rssi,
                 aps[i].secure ? "true" : "false");
        jw_put(&w, tail);
    }
    jw_put(&w, "]");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handler_save(httpd_req_t *req)
{
    if (req->content_len == 0 || req->content_len > FORM_BODY_MAX) {
        return send_result(req, "400 Bad Request", "提交内容无效",
                           "表单为空或过大,请返回重试。");
    }

    char body[FORM_BODY_MAX + 1];
    size_t received = 0;
    int retries = 0;

    while (received < (size_t)req->content_len) {
        const int chunk = httpd_req_recv(req, body + received,
                                        (size_t)req->content_len - received);
        if (chunk > 0) {
            received += (size_t)chunk;
            retries = 0;
            continue;
        }
        if (chunk == HTTPD_SOCK_ERR_TIMEOUT && retries < FORM_RECV_RETRY_MAX) {
            retries++;
            continue;
        }
        return send_result(req, "400 Bad Request", "接收中断", "请返回重试。");
    }
    body[received] = '\0';

    pc_config_t cfg;
    pc_config_set_defaults(&cfg);

    if (!http_form_has(body, received, "ssid") ||
        !http_form_get(body, received, "ssid", cfg.ssid, sizeof(cfg.ssid)) ||
        cfg.ssid[0] == '\0') {
        return send_result(req, "400 Bad Request", "Wi-Fi 名称无效",
                           "请填写 1-32 字节的 2.4GHz Wi-Fi SSID。");
    }

    // 密码允许为空(开放网络);字段存在但超长/含非法编码则拒绝。
    if (http_form_has(body, received, "pass")) {
        if (!http_form_get(body, received, "pass", cfg.password, sizeof(cfg.password))) {
            return send_result(req, "400 Bad Request", "Wi-Fi 密码无效",
                               "密码最长 63 个字符。");
        }
    } else {
        cfg.password[0] = '\0';
    }

    if (!http_form_has(body, received, "host") ||
        !http_form_get(body, received, "host", cfg.host, sizeof(cfg.host)) ||
        cfg.host[0] == '\0') {
        return send_result(req, "400 Bad Request", "电脑地址无效",
                           "请填写运行 AIDA64 的电脑局域网地址(最长 63 字符)。");
    }

    char port_text[8];
    if (http_form_get(body, received, "port", port_text, sizeof(port_text)) &&
        port_text[0] != '\0') {
        uint16_t port = 0;
        if (!http_form_get_u16(body, received, "port", &port) || port == 0) {
            return send_result(req, "400 Bad Request", "端口无效",
                               "端口应为 1-65535 的整数。");
        }
        cfg.port = port;
    }

    if (wifi_setup_sta_connect(&cfg) != ESP_OK) {
        return send_result(req, "500 Internal Server Error", "无法发起连接",
                           "设备网络栈异常,请返回重试或重启设备。");
    }
    if (!wifi_setup_sta_wait_connected(WIFI_SETUP_STA_VERIFY_MS)) {
        return send_result(req, "200 OK", "Wi-Fi 连接失败",
                           "请确认 SSID/密码正确、路由器为 2.4GHz,且信号良好后重试。");
    }

    const esp_err_t err = pc_config_save(&cfg);
    if (err != ESP_OK) {
        return send_result(req, "500 Internal Server Error", "保存配置失败",
                           "设备存储不可用,请重启后重试。");
    }

    s_configured = true;
    ESP_LOGI(TAG, "配网成功:SSID=%s PC=%s:%u", cfg.ssid, cfg.host, (unsigned)cfg.port);
    return send_result(req, "200 OK", "配置成功",
                       "设备已连接 Wi-Fi 并将开始监控。现在可以关闭此页面。");
}

static esp_err_t start_httpd(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 6;
    config.max_open_sockets = 3;   // 单用户配置页,按参考取值收敛
    config.lru_purge_enable = true;
    config.stack_size = 6144;

    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "启动配置网页失败: %s", esp_err_to_name(err));
        s_httpd = NULL;
        return err;
    }

    const httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handler_root,
        .user_ctx = NULL,
    };
    const httpd_uri_t uri_scan = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = handler_scan,
        .user_ctx = NULL,
    };
    const httpd_uri_t uri_save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = handler_save,
        .user_ctx = NULL,
    };

    err = httpd_register_uri_handler(s_httpd, &uri_root);
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(s_httpd, &uri_scan);
    }
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(s_httpd, &uri_save);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "注册配置网页路由失败: %s", esp_err_to_name(err));
        (void)httpd_stop(s_httpd);
        s_httpd = NULL;
        return err;
    }
    return ESP_OK;
}

static void stop_httpd(void)
{
    if (s_httpd != NULL) {
        (void)httpd_stop(s_httpd);
        s_httpd = NULL;
    }
}

// ---------------------------------------------------------------------------
// 配网态
// ---------------------------------------------------------------------------
esp_err_t wifi_setup_ap_start(void)
{
    if (!s_wifi_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_ap_active) {
        return ESP_OK;
    }

    // 进入配网前先断开旧 STA:否则旧的 BIT_STA_GOT_IP 会让凭据验证误判成功。
    wifi_setup_sta_disconnect();
    s_configured = false;

    wifi_config_t ap_cfg = {0};
    strlcpy((char *)ap_cfg.ap.ssid, WIFI_SETUP_AP_SSID, sizeof(ap_cfg.ap.ssid));
    ap_cfg.ap.ssid_len = (uint8_t)strlen(WIFI_SETUP_AP_SSID);
    strlcpy((char *)ap_cfg.ap.password, WIFI_SETUP_AP_PASS, sizeof(ap_cfg.ap.password));
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.max_connection = 1;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_cfg.ap.pmf_cfg.required = false;

    (void)esp_wifi_stop();
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    }
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "开启配网热点失败: %s", esp_err_to_name(err));
        (void)esp_wifi_set_mode(WIFI_MODE_STA);
        (void)esp_wifi_start();
        return err;
    }

    err = start_httpd();
    if (err != ESP_OK) {
        (void)esp_wifi_stop();
        (void)esp_wifi_set_mode(WIFI_MODE_STA);
        (void)esp_wifi_start();
        return err;
    }

    s_ap_active = true;
    ESP_LOGI(TAG, "配网热点已开启: SSID=%s 密码=%s 地址=%s",
             WIFI_SETUP_AP_SSID, WIFI_SETUP_AP_PASS, WIFI_SETUP_AP_URL);
    return ESP_OK;
}

void wifi_setup_ap_stop(void)
{
    if (!s_ap_active) {
        return;
    }

    stop_httpd();
    s_sta_want_connect = false;
    (void)esp_wifi_stop();
    (void)esp_wifi_set_mode(WIFI_MODE_STA);
    (void)esp_wifi_start();
    if (s_events != NULL) {
        xEventGroupClearBits(s_events, BIT_STA_GOT_IP);
    }

    s_ap_active = false;
    ESP_LOGI(TAG, "配网热点已关闭");
}

bool wifi_setup_ap_active(void)
{
    return s_ap_active;
}

int wifi_setup_scan(wifi_setup_ap_t *out, int max)
{
    if (!s_wifi_ready || out == NULL || max <= 0) {
        return -1;
    }
    if (max > WIFI_SETUP_SCAN_MAX) {
        max = WIFI_SETUP_SCAN_MAX;
    }

    // 扫描结果约 2KB:放静态缓冲,不占 httpd 任务栈,也不在无 PSRAM 的板子上堆分配。
    static wifi_ap_record_t records[WIFI_SETUP_SCAN_MAX];

    wifi_mode_t mode = WIFI_MODE_NULL;
    (void)esp_wifi_get_mode(&mode);
    ESP_LOGI(TAG, "开始扫描附近热点(mode=%d)", (int)mode);

    const wifi_scan_config_t scan_cfg = { .show_hidden = false };
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);  // 阻塞式,结束后结果在驱动里
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "扫描失败: %s", esp_err_to_name(err));
        (void)esp_wifi_clear_ap_list();
        return -1;
    }

    uint16_t found = WIFI_SETUP_SCAN_MAX;
    err = esp_wifi_scan_get_ap_records(&found, records);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "读取扫描结果失败: %s", esp_err_to_name(err));
        (void)esp_wifi_clear_ap_list();  // 未成功取走,主动释放驱动侧缓存
        return -1;
    }
    ESP_LOGI(TAG, "扫描完成,共 %u 个热点", (unsigned)found);

    const int n = found < (uint16_t)max ? (int)found : max;
    for (int i = 0; i < n; ++i) {
        memset(&out[i], 0, sizeof(out[i]));
        strlcpy(out[i].ssid, (const char *)records[i].ssid, sizeof(out[i].ssid));
        out[i].rssi = records[i].rssi;
        out[i].secure = records[i].authmode != WIFI_AUTH_OPEN;
    }

    // 按信号强度降序,页面直接从上往下展示。
    for (int i = 1; i < n; ++i) {
        const wifi_setup_ap_t key = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].rssi < key.rssi) {
            out[j + 1] = out[j];
            --j;
        }
        out[j + 1] = key;
    }
    return n;
}

bool wifi_setup_configured(void)
{
    return s_configured;
}

void wifi_setup_clear_configured(void)
{
    s_configured = false;
}
