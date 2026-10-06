// main/http_form.c —— urlencoded 字段解析实现(纯逻辑)。
#include "http_form.h"

#include <string.h>

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// 定位原始未解码的 value 区间。字段名按字面比较(表单字段名不含编码字符)。
static bool find_field(const char *body, size_t body_len, const char *key,
                       const char **value, size_t *value_len)
{
    const size_t key_len = strlen(key);
    size_t pos = 0;

    while (pos <= body_len) {
        size_t end = pos;
        while (end < body_len && body[end] != '&') {
            ++end;
        }

        const char *pair = body + pos;
        const size_t pair_len = end - pos;
        const char *eq = memchr(pair, '=', pair_len);
        if (eq != NULL) {
            const size_t name_len = (size_t)(eq - pair);
            if (name_len == key_len && memcmp(pair, key, key_len) == 0) {
                *value = eq + 1;
                *value_len = pair_len - name_len - 1;
                return true;
            }
        }

        if (end >= body_len) {
            break;
        }
        pos = end + 1;
    }
    return false;
}

bool http_form_has(const char *body, size_t body_len, const char *key)
{
    const char *value = NULL;
    size_t value_len = 0;

    if (body == NULL || key == NULL) {
        return false;
    }
    return find_field(body, body_len, key, &value, &value_len);
}

bool http_form_get(const char *body, size_t body_len, const char *key,
                   char *out, size_t out_size)
{
    if (body == NULL || key == NULL || out == NULL || out_size == 0) {
        return false;
    }

    const char *value = NULL;
    size_t value_len = 0;
    if (!find_field(body, body_len, key, &value, &value_len)) {
        out[0] = '\0';
        return false;
    }

    size_t written = 0;
    for (size_t i = 0; i < value_len; ++i) {
        char decoded;

        if (value[i] == '+') {
            decoded = ' ';
        } else if (value[i] == '%') {
            // 百分号编码必须完整且合法;残缺或非十六进制一律整体拒绝。
            if (i + 2 >= value_len) {
                out[0] = '\0';
                return false;
            }
            const int hi = hex_value(value[i + 1]);
            const int lo = hex_value(value[i + 2]);
            if (hi < 0 || lo < 0) {
                out[0] = '\0';
                return false;
            }
            decoded = (char)((hi << 4) | lo);
            i += 2;
        } else {
            decoded = value[i];
        }

        if (decoded == '\0') {
            out[0] = '\0';  // %00 无法存放在 C 字符串里,整体拒绝
            return false;
        }
        if (written + 1 >= out_size) {
            out[0] = '\0';  // 超长:整体拒绝,避免静默截断
            return false;
        }
        out[written++] = decoded;
    }

    out[written] = '\0';
    return true;
}

bool http_form_get_u16(const char *body, size_t body_len, const char *key,
                       uint16_t *out)
{
    char digits[8];

    if (out == NULL) {
        return false;
    }
    if (!http_form_get(body, body_len, key, digits, sizeof(digits))) {
        return false;
    }
    if (digits[0] == '\0') {
        return false;
    }

    uint32_t value = 0;
    for (const char *p = digits; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return false;
        }
        value = value * 10u + (uint32_t)(*p - '0');
        if (value > 65535u) {
            return false;
        }
    }

    *out = (uint16_t)value;
    return true;
}
