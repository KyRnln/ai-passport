<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

| 文件 | 类型 | 用途与来源 |
| --- | --- | --- |
| `fonts/fusion-pixel-12px-monospaced-zh_hans.ttf` | TTF，TrueType 轮廓，12px 网格 | 当前界面的源字体：缝合像素字体 12px 等宽、简体中文版（`Fusion Pixel 12px Mono zh-Hans`，2026.10.16，作者 TakWolf）。`name` 表 ID 13 声明为 **SIL Open Font License 1.1**，允许商用、嵌入与再分发，只需保留版权声明与许可文本；发布包同时附带它修补字形所用各项目的 OFL 文本（方舟像素、俐方體 11 號、Galmuri）。本文件不入库；请从 <https://github.com/TakWolf/fusion-pixel-font/releases> 下载 12px 等宽 TTF 压缩包。 |
| [`fonts/app_font_24.c`](fonts/app_font_24.c) | LVGL 9 字体，4 bpp（像素非全亮即全灭），24px | 全部主要文字：标题、指标数值、设置条目、顶栏与配网页正文。24px 恰好是下方设计网格的 2 倍。 |
| [`fonts/app_font_12.c`](fonts/app_font_12.c) | LVGL 9 字体，4 bpp（像素非全亮即全灭），12px | 提示、说明与提示页正文，即字体的设计网格字号。 |
| [`fonts/proggy_16.c`](fonts/proggy_16.c)、[`fonts/proggy_32.c`](fonts/proggy_32.c) | LVGL 9 字体，1 bpp，16/32px | 旧的纯 ASCII 像素字体，保留作参考，不再编入应用固件。 |
| [`fonts/ProggyCleanSZ-1.ttf`](fonts/ProggyCleanSZ-1.ttf) | TTF，TrueType 轮廓 | 保留的参考字体的源文件：ProggyClean，作者 Tristan Grimmer（2004），许可允许再分发；不直接烧录到设备。 |

这两个生成文件先后替换过两套旧字体：先是由**汉仪商业字体**栅格化出的子集（当时持有的是
个人非商用授权，不允许把字形数据随仓库再分发），后是 GNU Unifont 子集（16px 网格无法在
不产生灰边的前提下变小）。前者已删除，后者为改用更小网格而替换；它们仍存在于更早的历史
提交里。

当前字体由本仓库自带的转换脚本生成（fontTools + 内置扫描线栅格器）。本环境没有
Node，无法使用 `lv_font_conv`。在仓库根目录运行：

```bash
python tools/gen_cjk_font.py --font <路径>/fusion-pixel-12px-monospaced-zh_hans.ttf \
  --size 24 --name app_font_24 --output assets/fonts/app_font_24.c --snap
python tools/gen_cjk_font.py --font <路径>/fusion-pixel-12px-monospaced-zh_hans.ttf \
  --size 12 --name app_font_12 --output assets/fonts/app_font_12.c --snap
```

设计网格比文件名里的数字更重要：这份字体的 `upem = 1200`，每 100 单位一个设计像素，
因此 em 网格是 **12px**，所有轮廓坐标都是 100 的整数倍。字号必须按 12 的整数倍生成
（界面用 24px 作主字、12px 作提示），否则一个设计像素会映射到非整数屏幕像素，边缘被
抗锯齿成灰边。`--snap` 点对点模式还会把覆盖度二值化为全亮/全灭，并把字宽取整到整数
像素，使字形正好落在面板像素网格上。行高与基线取自子集的实际墨迹范围（另加 2px 留白），
而不是字体自带的行高参数：24px 下墨迹在基线上方 18px、下方 4px（12px 下是 9px / 2px），
因此两个字号分别是 `line_height` 24/13、`base_line` 4/2。半宽拉丁字形在 24px 下只有
16px 高，而汉字上升部为 18px，共用基线时纯拉丁的标签（指标名称/数值等）会比汉字低
2px，因此 `main/pc_ui.c` 把这些标签的墨迹在其所占区间内上下居中。两个字号合计约 82 KB
Flash（字形位图 65 KB + 16 KB），在 `main/CMakeLists.txt` 注册。

字符清单直接取自 `main/*.c` 的字符串字面量（忽略注释），改文案后重新生成会直接报出
缺字；等宽版省掉了一些复杂字形，若将来报缺字，改用
`fusion-pixel-12px-proportional-zh_hans.ttf` 即可。`--print-inventory` 可列出覆盖的字符，
`--dump-codepoint` 可在没有设备时打印单个字形的位图。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。
