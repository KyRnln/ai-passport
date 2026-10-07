// main/pc_metrics.c —— AIDA64 RemoteSensor /sse 解析实现(纯逻辑,可主机测试)。
#include "pc_metrics.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define PC_SEP "{|}"
#define PC_SEP_LEN 3
#define PC_DATA_PREFIX "data:"
#define PC_DATA_PREFIX_LEN 5

void pc_metrics_parser_init(pc_metrics_parser_t *p)
{
    if (p == NULL) {
        return;
    }
    memset(p, 0, sizeof(*p));
}

// 不区分大小写的子串查找。needle 可为 UTF-8 中文(逐字节比较,不受 locale 影响)。
static bool contains_ci(const char *hay, const char *needle)
{
    const size_t n = strlen(needle);
    if (n == 0) {
        return true;
    }
    for (const char *h = hay; *h != '\0'; ++h) {
        size_t i = 0;
        while (i < n && h[i] != '\0' &&
               tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) {
            ++i;
        }
        if (i == n) {
            return true;
        }
        if (h[i] == '\0') {
            break;
        }
    }
    return false;
}

// 取文本中最后一个数字。AIDA64 记录形如 "<Label> <value> <unit>",
// 取最后一个可避开 Label 里可能出现的 "#1" 之类的编号。
static bool last_number(const char *s, float *out)
{
    bool found = false;
    const char *p = s;

    while (*p != '\0') {
        if (*p >= '0' && *p <= '9') {
            char *end = NULL;
            const double value = strtod(p, &end);
            if (end == p) {
                ++p;
                continue;
            }
            found = true;
            *out = (float)value;
            p = end;
        } else {
            ++p;
        }
    }
    return found;
}

typedef enum {
    KIND_NONE = 0,
    KIND_CPU,
    KIND_GPU,
    KIND_MEM,
    KIND_UPTIME,
} pc_kind_t;

static pc_kind_t classify_record(const char *text)
{
    // AIDA64 的系统运行时间项没有利用率类关键词,先单独识别。
    // 中文标签(AIDA64 与设备之间按 UTF-8 传输)同样命中。
    if (contains_ci(text, "uptime") || contains_ci(text, "up time") ||
        contains_ci(text, "运行时间") || contains_ci(text, "开机时间")) {
        return KIND_UPTIME;
    }

    const bool util = contains_ci(text, "util") || contains_ci(text, "usage") ||
                      contains_ci(text, "load") || contains_ci(text, "rate") ||
                      contains_ci(text, "occup") || contains_ci(text, "占用") ||
                      contains_ci(text, "使用率") || contains_ci(text, "使用");

    if (!util) {
        return KIND_NONE;
    }
    if (contains_ci(text, "cpu") || contains_ci(text, "处理器")) {
        return KIND_CPU;
    }
    if (contains_ci(text, "gpu") || contains_ci(text, "显卡")) {
        return KIND_GPU;
    }
    if (contains_ci(text, "memory") || contains_ci(text, "ram") || contains_ci(text, "内存")) {
        return KIND_MEM;
    }
    return KIND_NONE;
}

// 解析 AIDA64 时长文本,如 "1:23:45"、"0:12"、"1 day 2:03:04" → 秒。
// 约定:两段按 h:mm(运行时间不会用 mm:ss 表示),三段按 h:mm:ss,
// 前面若出现 "day" 则把紧邻的整数按天累加。
static bool parse_uptime(const char *text, uint32_t *out)
{
    const char *colon = strrchr(text, ':');
    if (colon == NULL) {
        return false;
    }

    // 向前收拢成 [数字|:] 的最长片段,即时间串起点。
    const char *start = colon;
    while (start > text &&
           (isdigit((unsigned char)start[-1]) || start[-1] == ':')) {
        --start;
    }
    if (!isdigit((unsigned char)*start)) {
        return false;
    }
    const char *end = start;
    while (isdigit((unsigned char)*end) || *end == ':') {
        ++end;
    }

    uint32_t parts[3] = {0, 0, 0};
    int count = 0;
    const char *q = start;
    while (q < end && count < 3) {
        if (!isdigit((unsigned char)*q)) {
            return false;   // 连续 ':' 视为非法
        }
        uint32_t acc = 0;
        while (q < end && isdigit((unsigned char)*q)) {
            acc = acc * 10u + (uint32_t)(*q - '0');
            ++q;
        }
        parts[count++] = acc;
        if (q < end) {
            ++q;            // 跳过 ':'
        }
    }
    if (q != end || (count != 2 && count != 3)) {
        return false;
    }

    uint32_t total = (count == 3) ? parts[0] * 3600u + parts[1] * 60u + parts[2]
                                  : parts[0] * 3600u + parts[1] * 60u;

    // "1 day 2:03:04":时间串之前若写着 day,把紧邻的整数当天数。
    if (contains_ci(text, "day")) {
        const char *d = start;
        while (d > text && !isdigit((unsigned char)d[-1])) {
            --d;
        }
        if (d > text) {
            const char *ds = d;
            while (ds > text && isdigit((unsigned char)ds[-1])) {
                --ds;
            }
            uint32_t days = 0;
            for (const char *r = ds; r < d; ++r) {
                days = days * 10u + (uint32_t)(*r - '0');
            }
            total += days * 86400u;
        }
    }

    *out = total;
    return true;
}

static bool apply_metrics(pc_metrics_parser_t *p, const char *content)
{
    const pc_kind_t kind = classify_record(content);
    if (kind == KIND_NONE) {
        return false;
    }

    pc_metrics_t *m = &p->metrics;

    if (kind == KIND_UPTIME) {
        uint32_t seconds = 0;
        if (!parse_uptime(content, &seconds)) {
            return false;
        }
        m->uptime_sec = seconds;
        m->uptime_valid = true;
        p->updates++;
        return true;
    }

    float value = 0.0f;
    if (!last_number(content, &value)) {
        return false;
    }
    if (value < 0.0f) {
        value = 0.0f;
    }
    if (value > 100.0f) {
        value = 100.0f;
    }

    switch (kind) {
    case KIND_CPU:
        m->cpu_pct = value;
        m->cpu_valid = true;
        break;
    case KIND_GPU:
        m->gpu_pct = value;
        m->gpu_valid = true;
        break;
    case KIND_MEM:
        m->mem_pct = value;
        m->mem_valid = true;
        break;
    default:
        return false;
    }
    p->updates++;
    return true;
}

static void push_record_byte(pc_metrics_parser_t *p, char c)
{
    if (p->seg_overflow) {
        return;
    }
    if (p->seg_len + 1 >= PC_METRICS_SEG_MAX) {
        p->seg_overflow = true;
        return;
    }
    p->seg[p->seg_len++] = c;
}

// 结束当前记录:按第一个 '|' 切成 id / content,只对 content 做识别。
static bool finish_record(pc_metrics_parser_t *p)
{
    bool updated = false;

    if (!p->seg_overflow) {
        p->seg[p->seg_len] = '\0';
        const char *bar = memchr(p->seg, '|', p->seg_len);
        if (bar != NULL) {
            updated = apply_metrics(p, bar + 1);
        }
    }

    p->seg_len = 0;
    p->seg_overflow = false;
    return updated;
}

bool pc_metrics_parser_feed(pc_metrics_parser_t *p, const char *data, size_t len)
{
    if (p == NULL || data == NULL) {
        return false;
    }

    bool updated = false;

    for (size_t i = 0; i < len; ++i) {
        const char c = data[i];

        if (c == '\n') {
            if (p->in_data) {
                updated |= finish_record(p);
            }
            p->seg_len = 0;
            p->seg_overflow = false;
            p->sep_pos = 0;
            p->prefix_pos = 0;
            p->in_data = false;
            p->skip_line = false;
            p->skip_space = false;
            continue;
        }
        if (c == '\r' || p->skip_line) {
            continue;
        }

        if (!p->in_data) {
            if (p->prefix_pos < PC_DATA_PREFIX_LEN && c == PC_DATA_PREFIX[p->prefix_pos]) {
                p->prefix_pos++;
                if (p->prefix_pos == PC_DATA_PREFIX_LEN) {
                    p->in_data = true;
                    p->skip_space = true;
                }
            } else {
                p->skip_line = true;  // 非 data: 行(空行/注释/event: 等)整行忽略
            }
            continue;
        }

        if (p->skip_space) {
            p->skip_space = false;
            if (c == ' ') {
                continue;
            }
        }

        if (c == PC_SEP[p->sep_pos]) {
            p->sep_pos++;
            if (p->sep_pos == PC_SEP_LEN) {
                p->sep_pos = 0;
                updated |= finish_record(p);
            }
            continue;
        }

        if (p->sep_pos > 0) {
            // 分隔符只匹配了一部分:把已匹配的字节当普通内容补回,再处理当前字节。
            const size_t matched = p->sep_pos;
            p->sep_pos = 0;
            for (size_t k = 0; k < matched; ++k) {
                push_record_byte(p, PC_SEP[k]);
            }
        }
        push_record_byte(p, c);
    }

    return updated;
}
