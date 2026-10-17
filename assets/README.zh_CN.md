<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

| 文件 | 类型 | 用途与来源 |
| --- | --- | --- |
| `fonts/HYPixel11pxU-2.ttf` | TTF，TrueType 轮廓 | 当前界面的源字体：汉仪像素 11px U（`HYPixel 11px U`，Version 9.00.00），11px 像素网格的中文字体。属**汉仪商业字体**：`name` 表 ID 13 指向 `http://hanyi.com.cn/law.php` 的授权说明，嵌入/再分发字形数据需取得汉仪授权。本文件不入库；重新生成时请自备有授权的副本。 |
| [`fonts/app_font_26.c`](fonts/app_font_26.c) | LVGL 9 字体，4 bpp（像素非全亮即全灭），26px | 全部主要文字：标题、指标数值、设置条目、顶栏与配网页正文。26px 恰好是下方设计网格的 2 倍。 |
| [`fonts/app_font_13.c`](fonts/app_font_13.c) | LVGL 9 字体，4 bpp（像素非全亮即全灭），13px | 提示、说明与提示页正文，即字体的设计网格字号。 |
| [`fonts/proggy_16.c`](fonts/proggy_16.c)、[`fonts/proggy_32.c`](fonts/proggy_32.c) | LVGL 9 字体，1 bpp，16/32px | 旧的纯 ASCII 像素字体，保留作参考，不再编入应用固件。 |

### 生成字体的授权状态

`fonts/app_font_26.c` 与 `fonts/app_font_13.c` 是上表汉仪字体的**派生数据**：其中的位图
就是汉仪字形轮廓的栅格化结果，因此与源字体受同一份授权约束。这两个文件已提交进本仓库，
由它们编出的固件同样包含这些字形数据。

本项目使用的字体副本来自汉仪的**个人非商用授权**（`hanyi.com.cn/faq-doc-2`），仅限个人
单机版用于学习、研究、欣赏，不可作任何商业发布或其他盈利性使用。汉仪官方条款明确：除
个人非商业使用以外的任何使用方式，均需另行取得汉仪的书面许可（`hanyi.com.cn/license`）。
因此把本仓库发布到平台、发布内嵌这些字形的固件，或任何商业使用，都超出了现有授权范围。

在公开发布或分发本项目之前，请二选一：为预期用途取得汉仪的书面授权，或把这两个文件换成
允许嵌入与再分发的字体。符合本仓库"整数倍设计网格"流程的中文开源像素字体可选缝合像素字体
（Fusion Pixel Font，OFL-1.1，8/10/12px）与方舟像素字体（Ark Pixel Font，MIT / OFL-1.1，
10/12/16px）；两者都允许商用与嵌入，但需保留其版权声明与许可文本。

只删掉这两个生成文件并不能让仓库变得可分发的：`main/CMakeLists.txt` 会编译它们，因此必须
换成其他字体或在本地生成；而且已推送的历史提交里仍然保留着这些字形数据。

当前字体由本仓库自带的转换脚本生成（fontTools + 内置扫描线栅格器）。本环境没有
Node，无法使用 `lv_font_conv`。在仓库根目录运行：

```bash
python tools/gen_cjk_font.py --font <路径>/HYPixel11pxU-2.ttf \
  --size 26 --name app_font_26 --output assets/fonts/app_font_26.c --snap
python tools/gen_cjk_font.py --font <路径>/HYPixel11pxU-2.ttf \
  --size 13 --name app_font_13 --output assets/fonts/app_font_13.c --snap
```

设计网格比字体名更重要：`HYPixel 11px U` 的 `upem = 1000`，每 77 单位一个设计像素，
因此它的 em 网格是 **13px**（77 × 13 = 1001），而不是 11px。按 11px 或 22px 生成会把
一个设计像素映射到 1.69 个屏幕像素，字形边缘必然被抗锯齿成灰边；应按 13 的整数倍
生成。`--snap` 点对点模式还会把覆盖度二值化为全亮/全灭，并把字宽取整到整数像素，
使字形正好落在面板像素网格上。行高与基线取自子集的实际墨迹范围（另加 2px 留白），
而不是字体自带的 `hhea`——后者 1.31em 的行框会把文字在框内整体往下推。两个字号合计
约 90 KB Flash，在 `main/CMakeLists.txt` 注册。

字符清单直接取自 `main/*.c` 的字符串字面量（忽略注释），改文案后重新生成会直接报出
缺字；`--print-inventory` 可列出覆盖的字符，`--dump-codepoint` 可在没有设备时打印
单个字形的位图。

[`fonts/ProggyCleanSZ-1.ttf`](fonts/ProggyCleanSZ-1.ttf) 是保留的参考字体的源文件：
ProggyClean，作者 Tristan Grimmer（2004），许可允许再分发；不直接烧录到设备。

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
