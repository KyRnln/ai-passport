<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# AIDA64 PC 状态监视器

把 AI Passport 变成 Windows 主机的随身状态屏。设备接入你的 Wi-Fi，连接
AIDA64 的 RemoteSensor 数据流，在 320 x 240 横屏上以黑底白字的点阵终端风格实时
显示 CPU / GPU / 内存占用率，以及 PC 的已运行时间。

本分支（`feature/aida64-pc-monitor`）只包含这个应用。上游基线、硬件能力契约
与文档索引仍在 [docs/README.zh_CN.md](docs/README.zh_CN.md)。

## 功能

- **占用率卡片**：CPU / GPU / 内存百分比，每张卡片是「数值 + 整宽进度条」；
  进度条在 100% 时变红。
- **顶栏**：左侧是 PC 已运行时间（`HH:MM:SS`，AIDA64 原始值含天数时才显示 `xxD `
  前缀），右侧是电量。插上会枚举数据的 USB 口时，电量左侧显示绿色 USB 标志；
  纯充电器只给 5V、不枚举数据，因此不显示。
- **数字滚动**：数值、运行时间与电量按单个数字滚动更新，且增减方向不同时滚动方向
  也不同。
- **本地刷新兜底**：超过 2 秒没有新数据时，各项数值与运行时间显示 `-`，
  避免把卡死的主机当成实时状态。
- **连接状态常显**：左下角始终显示 `无 Wi-Fi` / `已连接` / `连接中` /
  `PC 离线` 之一。
- **设置页**：屏幕亮度（10% 步进，10%~100%）、Wi-Fi 配网、横屏方向、设备信息。
  亮度与方向写入 NVS，开机自动恢复。
- **SoftAP 配网**：开启 `AIPassport-Setup` 热点，用手机扫描周边网络、选择 SSID，
  再填入密码与 PC 地址，配置保存在 NVS；配网页为黑底点阵风格。
- **终端观感**：纯黑背景、白色文字、全部直角。文字使用缝合像素字体的 24px 与
  12px 子集，存放于 `assets/fonts/app_font_24.c` 与 `assets/fonts/app_font_12.c`；
  两者都按字体 12px 设计网格的整数倍生成，因此是点对点显示。源字体及其
  [OFL-1.1 授权](assets/README.zh_CN.md#字库fonts)见资源目录说明。

## 交互

三个按键为 `UP`、`DOWN`、`OK`。

**监控主页**

- `OK` 长按：进入设置页；短按不动作。

**设置页**

- `UP` / `DOWN`：移动选择，当前行显示 2px 白色光标框。
- `OK`：进入当前条目。
- `OK` 长按：返回监控主页。
- `亮度`：进入后光标框转为绿色，`UP` / `DOWN` 以 10% 步进调整，`OK` 结束调节。
- `方向`：`OK` 在两种横屏方向之间翻转 180° 并保存。
- `Wi-Fi`：`OK` 进入二次确认页，确认页 `OK` 开始配网，`UP` / `DOWN` 取消。
- `设备信息`：只读页，显示已配置的 PC 地址与需要添加的 AIDA64 传感器名称。

**配网态**（确认 Wi-Fi 配网后进入）

- 停止监控，设备开启 `AIPassport-Setup` 热点。
- 手机打开 `http://192.168.4.1`，扫描并保存网络。
- `OK` 长按：不保存直接退出配网。

## AIDA64 设置

1. 在 AIDA64 中启用 RemoteSensor / LCD 的 HTTP 服务，记下端口（默认 80）。
2. 添加能让解析器识别的简单传感器项：
   - `CPU` + 利用率类词，例如 `CPU Utilization`
   - `GPU` + 利用率类词
   - `Memory`（或 `RAM`）+ 利用率类词
   - 标签含 `UpTime`（或本地化的「运行时间」）的运行时间项
3. 在设备配网页填入 PC 的地址与端口。

解析器读取 `/sse` 流的 `data:` 记录，按 `{|}` 切分，并取记录中最后一个数字
作为百分比。

## 说明

- 面板物理尺寸是 240 x 320 竖屏；BSP 交换 X/Y 使本应用以 320 x 240 横屏运行。
  若你的安装方式下画面上下颠倒，用 `Rotation` 翻转即可。
- 利用率标签同时匹配英文与部分本地化写法，用英文标签最通用。
- 扫描 Wi-Fi 时射频切换信道，配网热点会短暂停顿。
- 配网页的 Wi-Fi 扫描在点击处理修复后尚未在设备上复测；本文其它行为均已在
  硬件上确认。
- 从 `0x0` 刷入合并的 `full.bin` 会重写 NVS，因而清空已保存的 Wi-Fi 配置；
  只刷 `0x10000` 的应用镜像可以保留。

## 来源

- 仓库 `KyRnln/ai-passport`，分支 `feature/aida64-pc-monitor`。
- 基于上游 FoloToy AI Passport 基线开发。硬件能力契约见
  [docs/README.zh_CN.md](docs/README.zh_CN.md)，AI 开发流程见
  [docs/development/ai-guide.zh_CN.md](docs/development/ai-guide.zh_CN.md)。
