// main/http_form.h —— application/x-www-form-urlencoded 字段解析(纯逻辑)。
//
// 配网页面是外部输入边界:字段长度、百分号编码、数值溢出都必须显式校验,
// 不能依赖上层"看起来合法"。本文件不依赖 ESP-IDF,便于主机测试。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 在 urlencoded body 中查找 key 并解码其值(支持 '+' 表示空格,支持 %XX)。
// out 一定会被写入并以 NUL 结尾(out_size>0 时)。
// 返回 false 的情况:未找到该字段、编码非法、解码后含 %00、或超出 out_size-1。
bool http_form_get(const char *body, size_t body_len, const char *key,
                   char *out, size_t out_size);

// 字段是否出现(不解码、不看长度)。用于把"字段缺失"与"字段存在但非法"
// 区分成不同的用户提示。
bool http_form_has(const char *body, size_t body_len, const char *key);

// 读取十进制 u16 字段。拒绝空串、非数字、以及 >65535 的值。
bool http_form_get_u16(const char *body, size_t body_len, const char *key,
                       uint16_t *out);
