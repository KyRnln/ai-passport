// tests/test_http_form.c —— 配网表单解析(外部输入边界)测试。
#include <assert.h>
#include <string.h>

#include "http_form.h"

static const char *BODY = "ssid=MyNet&pass=abc123&host=192.168.1.10&port=80";

static void expect_value(const char *body, const char *key, const char *expected)
{
    char out[64];
    assert(http_form_get(body, strlen(body), key, out, sizeof(out)));
    assert(strcmp(out, expected) == 0);
}

int main(void)
{
    char out[64];

    expect_value(BODY, "ssid", "MyNet");
    expect_value(BODY, "pass", "abc123");
    expect_value(BODY, "host", "192.168.1.10");

    // 百分号解码与 '+' 空格。
    const char *encoded = "ssid=My%20Net&pass=a%2Bb%26c&host=x+y";
    expect_value(encoded, "ssid", "My Net");
    expect_value(encoded, "pass", "a+b&c");
    expect_value(encoded, "host", "x y");

    // 字段缺失必须失败,而不是返回空串。
    assert(!http_form_get(BODY, strlen(BODY), "nope", out, sizeof(out)));
    assert(!http_form_has(BODY, strlen(BODY), "nope"));
    assert(http_form_has(BODY, strlen(BODY), "ssid"));

    // 字段名不能被前缀误命中。
    const char *prefix = "ssid_extra=1&host=2.2.2.2";
    assert(!http_form_has(prefix, strlen(prefix), "ssid"));
    assert(http_form_has(prefix, strlen(prefix), "ssid_extra"));

    // 空值字段存在但内容为空。
    const char *empty_pass = "ssid=A&pass=&host=1.1.1.1";
    assert(http_form_has(empty_pass, strlen(empty_pass), "pass"));
    assert(http_form_get(empty_pass, strlen(empty_pass), "pass", out, sizeof(out)));
    assert(out[0] == '\0');

    // 恰好填满与超长:容量 5 时最多 4 个字符。
    assert(http_form_get("k=abcd", 6, "k", out, 5));
    assert(strcmp(out, "abcd") == 0);
    assert(!http_form_get("k=abcde", 7, "k", out, 5));
    assert(out[0] == '\0');

    // 非法编码与 %00 必须整体拒绝。
    assert(!http_form_get("k=a%ZZ", 6, "k", out, sizeof(out)));
    assert(!http_form_get("k=a%0", 5, "k", out, sizeof(out)));
    assert(!http_form_get("k=a%00b", 7, "k", out, sizeof(out)));

    // 没有 '=' 的噪声段不影响后续字段。
    expect_value("junk&host=10.0.0.5", "host", "10.0.0.5");

    // u16:合法边界、非法输入、溢出。
    uint16_t port = 0;
    assert(http_form_get_u16(BODY, strlen(BODY), "port", &port) && port == 80);
    assert(http_form_get_u16("port=65535", 10, "port", &port) && port == 65535);
    assert(http_form_get_u16("port=1", 6, "port", &port) && port == 1);
    assert(http_form_get_u16("port=0", 6, "port", &port) && port == 0);
    assert(!http_form_get_u16("port=", 5, "port", &port));
    assert(!http_form_get_u16("port=abc", 8, "port", &port));
    assert(!http_form_get_u16("port=65536", 10, "port", &port));
    assert(!http_form_get_u16("port=-1", 7, "port", &port));
    assert(!http_form_get_u16("port=12x", 8, "port", &port));
    assert(!http_form_get_u16("host=1.1.1.1", 12, "port", &port));

    // 空 body 不得崩溃。
    assert(!http_form_get("", 0, "ssid", out, sizeof(out)));
    assert(!http_form_get(NULL, 0, "ssid", out, sizeof(out)));

    return 0;
}
