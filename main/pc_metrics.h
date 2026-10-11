// main/pc_metrics.h —— AIDA64 RemoteSensor /sse 报文解析(纯逻辑)。
//
// 该文件刻意不依赖 ESP-IDF / LVGL / FreeRTOS,以便在主机上直接跑单元测试。
//
// AIDA64 RemoteSensor(/sse)的响应是 SSE 长连接,每帧形如:
//   data: Page0|{|}Simple1|CPU Utilization 12 %{|}Simple2|GPU Utilization 0 %{|}Simple3|Memory Utilization 56 %
// 记录之间用 "{|}" 分隔;每段内部是 "<元素id>|<显示文本>"。
// 显示文本由 AIDA64 里 LCD 项目的 Label + 值 + 单位拼成,例如 "CPU Utilization 12 %"。
//
// 识别策略:按关键词匹配 Label(cpu/gpu/memory + 利用率类词),再取文本中
// 【最后一个数字】作为百分比 —— 取最后一个而不是第一个,是为了兼容
// "CPU #1 Utilization 12 %" 这类 Label 自带数字的情况。
//
// 另识别 AIDA64 的"系统运行时间"项(Label 含 "UpTime"/"Up Time"/"运行时间"/
// "开机时间"):该值是时长文本(如 "1:23:45"、"1 day 2:03:04" 或 "3d 00:34:13"),
// 按 h:mm[:ss] 解析成总秒数(天数前缀一并累加),顶栏用它显示 PC 已运行时间。
//
// 重要:本解析器【不按整行缓冲】。AIDA64 一帧里包含用户添加的全部 LCD 项,
// 项多时该行可达数 KB;按行缓存会在无 PSRAM 的板子上浪费内存。这里改为
// 按 "{|}" 记录切分,只缓冲单条记录(PC_METRICS_SEG_MAX),内存占用与项数无关。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 一次采样结果。*_valid 为 false 表示自建立连接以来还没收到过该项。
typedef struct {
    float cpu_pct;
    float gpu_pct;
    float mem_pct;
    bool cpu_valid;
    bool gpu_valid;
    bool mem_valid;
    uint32_t uptime_sec;    // PC 已运行时间(秒),来自 AIDA64 的 UpTime 项
    bool uptime_valid;
} pc_metrics_t;

// 单条记录缓冲上限。一条记录只是 "id|label value unit",160 字节足够宽松;
// 超出只丢弃该条记录,不影响同帧内其它记录。
#define PC_METRICS_SEG_MAX 160

typedef struct {
    pc_metrics_t metrics;

    char seg[PC_METRICS_SEG_MAX];  // 当前记录的原始字节(id|content)
    size_t seg_len;
    bool seg_overflow;             // 当前记录超长,整条丢弃

    size_t sep_pos;                // 正在匹配的 "{|}" 分隔符进度
    size_t prefix_pos;             // 行首 "data:" 前缀匹配进度
    bool in_data;                  // 当前行是 data: 行,其内容计入记录
    bool skip_line;                // 当前行不是 data: 行,整行忽略
    bool skip_space;               // "data:" 之后跳过一个空格

    uint32_t updates;              // 累计成功应用了多少个指标值
} pc_metrics_parser_t;

// 复位解析器(清空状态与采样值)。
void pc_metrics_parser_init(pc_metrics_parser_t *p);

// 送入一段 SSE 原始字节。TCP 会任意切分数据,本函数可接受任意边界的分片。
// 返回:本次调用是否至少应用了一个新的指标值。
bool pc_metrics_parser_feed(pc_metrics_parser_t *p, const char *data, size_t len);

// 把运行时间格式化为顶栏文案 "xxD HH:MM:SS"(天数恒显示,时分秒补零)。
// 返回 snprintf 语义的长度(不含结尾 '\0');返回值 >= buf_len 表示被截断。
size_t pc_uptime_format(uint32_t uptime_sec, char *buf, size_t buf_len);
