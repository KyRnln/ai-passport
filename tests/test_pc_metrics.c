// tests/test_pc_metrics.c —— AIDA64 /sse 解析器的纯逻辑测试。
#include <assert.h>
#include <string.h>

#include "pc_metrics.h"

static void feed_str(pc_metrics_parser_t *p, const char *s)
{
    (void)pc_metrics_parser_feed(p, s, strlen(s));
}

// 逐字节送入,模拟 TCP 在任意位置切分数据。
static void feed_slowly(pc_metrics_parser_t *p, const char *s)
{
    for (const char *c = s; *c != '\0'; ++c) {
        (void)pc_metrics_parser_feed(p, c, 1);
    }
}

static const char FRAME[] =
    "data: Page0|{|}Simple1|CPU Utilization 12 %{|}Simple2|GPU Utilization 0 %"
    "{|}Simple3|Memory Utilization 56 %\n\n";

int main(void)
{
    // 初始状态:全部无效。
    pc_metrics_parser_t p;
    pc_metrics_parser_init(&p);
    assert(!p.metrics.cpu_valid && !p.metrics.gpu_valid && !p.metrics.mem_valid);

    // 完整一帧,一次性送入。
    assert(pc_metrics_parser_feed(&p, FRAME, strlen(FRAME)));
    assert(p.metrics.cpu_valid && p.metrics.cpu_pct == 12.0f);
    assert(p.metrics.gpu_valid && p.metrics.gpu_pct == 0.0f);
    assert(p.metrics.mem_valid && p.metrics.mem_pct == 56.0f);
    assert(p.updates == 3);

    // 相同一帧逐字节送入(跨分隔符、跨记录边界都必须正确)。
    pc_metrics_parser_init(&p);
    feed_slowly(&p, FRAME);
    assert(p.metrics.cpu_pct == 12.0f);
    assert(p.metrics.gpu_pct == 0.0f);
    assert(p.metrics.mem_pct == 56.0f);

    // 无空格的 "data:Page0|...",且分隔符恰好落在分片接缝上。
    pc_metrics_parser_init(&p);
    const char frame2[] = "data:Page0|{|}Simple1|CPU Utilization 7 %{|}Simple2|Memory Utilization 33 %\n";
    assert(pc_metrics_parser_feed(&p, frame2, 24));
    assert(pc_metrics_parser_feed(&p, frame2 + 24, strlen(frame2) - 24));
    assert(p.metrics.cpu_pct == 7.0f);
    assert(p.metrics.mem_pct == 33.0f);
    assert(!p.metrics.gpu_valid);

    // CRLF 行尾。
    pc_metrics_parser_init(&p);
    feed_str(&p, "data: Page0|{|}Simple1|GPU Utilization 88 %\r\n");
    assert(p.metrics.gpu_pct == 88.0f);

    // Label 自带编号时,取最后一个数字而不是 "#1"。
    pc_metrics_parser_init(&p);
    feed_str(&p, "data: Page0|{|}Simple1|CPU #1 Utilization 45 %\n");
    assert(p.metrics.cpu_pct == 45.0f);

    // 小数与上下限收敛。
    pc_metrics_parser_init(&p);
    feed_str(&p, "data: Page0|{|}Simple1|Memory Utilization 56.5 %\n");
    assert(p.metrics.mem_pct == 56.5f);
    feed_str(&p, "data: Page0|{|}Simple1|CPU Utilization 250 %\n");
    assert(p.metrics.cpu_pct == 100.0f);

    // 非利用率项必须被忽略:频率、时钟、温度、网速都不能误命中。
    pc_metrics_parser_init(&p);
    feed_str(&p,
             "data: Page0|{|}Simple1|CPU Clock 3890 MHz{|}Simple2|GPU Clock 1800 MHz"
             "{|}Simple3|GPU1 Memory Clock 7000 MHz{|}Simple4|NIC1 Download Rate 120 KB/s"
             "{|}Simple5|CPU Temperature 55 C{|}Simple6|Memory Speed DDR4-3200\n\n");
    assert(!p.metrics.cpu_valid && !p.metrics.gpu_valid && !p.metrics.mem_valid);
    assert(p.updates == 0);

    // 非 data: 行(SSE 注释/event/retry/HTTP 头)整体忽略。
    pc_metrics_parser_init(&p);
    feed_str(&p, ": keep-alive\nevent: message\nretry: 3000\nid: 4\n");
    assert(p.updates == 0);

    // 单条记录超长只丢弃该条,不影响后续记录。
    pc_metrics_parser_init(&p);
    feed_str(&p, "data: Page0|{|}Simple1|CPU ");
    for (int i = 0; i < PC_METRICS_SEG_MAX + 20; ++i) {
        feed_str(&p, "x");
    }
    feed_str(&p, "{|}Simple2|GPU Utilization 61 %\n");
    assert(p.metrics.gpu_pct == 61.0f);
    assert(!p.metrics.cpu_valid);

    // 分片恰好停在 data: 行前缀中间。
    pc_metrics_parser_init(&p);
    assert(!pc_metrics_parser_feed(&p, "da", 2));
    const char rest[] = "ta: Page0|{|}S1|CPU Utilization 5 %\n";
    assert(pc_metrics_parser_feed(&p, rest, strlen(rest)));
    assert(p.metrics.cpu_pct == 5.0f);

    // 多帧连续刷新:后一帧覆盖前一帧。
    pc_metrics_parser_init(&p);
    feed_str(&p, "data: Page0|{|}S1|CPU Utilization 10 %\n");
    feed_str(&p, "data: Page0|{|}S1|CPU Utilization 90 %\n");
    assert(p.metrics.cpu_pct == 90.0f);
    assert(p.updates == 2);

    return 0;
}
