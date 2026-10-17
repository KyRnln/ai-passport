<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# AIDA64 PC Monitor

Turn the AI Passport into a wearable status display for a Windows PC. The
device joins your Wi-Fi, connects to AIDA64's RemoteSensor stream, and shows
live CPU, GPU, and memory utilization plus the PC's uptime on a 320 x 240
landscape screen in a black-on-white dot-matrix terminal style.

This branch (`feature/aida64-pc-monitor`) contains the application only. The
upstream baseline, hardware contract, and documentation index stay in
[docs/README.md](docs/README.md).

## Getting started

1. Flash the firmware: use the merged `FoloToy-AI-Passport-full.bin` at `0x0` for a
   blank board or when you want to clear the stored configuration, or the
   application-only `FoloToy-AI-Passport.bin` at `0x10000` to keep it.
2. On the device, long press `OK` to open the settings page.
3. Select the Wi-Fi row; press `OK` on the confirmation page to start provisioning,
   and the device raises the `AIPassport-Setup` hotspot.
4. Join that hotspot from a phone (password `aipassport`), open
   `http://192.168.4.1`, scan for and pick your 2.4 GHz network, then enter its
   password plus the address and port of the PC running AIDA64, and save.
5. Once the Wi-Fi check passes, the device returns to the monitor page: PC uptime on
   the left of the header, battery on the right, and the CPU, GPU and memory cards
   below. The bottom-left status switching from connecting to connected means data is
   flowing; if it stays on PC offline, recheck the labels described in the AIDA64
   section below.
6. To change the configuration later, long press `OK` and pick the Wi-Fi row; to
   change the screen orientation, use the Rotation row.

## What it does

- **Utilization cards**: CPU, GPU, and memory percentage, each drawn as a
  value line plus a full-width bar. A bar turns red at 100 %.
- **Header**: PC uptime on the left as `HH:MM:SS`, with the `xxD ` day prefix
  only when AIDA64 reports days, and the battery percentage on the right. A green
  USB logo appears left of the battery while the device is plugged into a USB
  host that enumerates data; a charge-only adapter supplies 5 V without
  enumerating, so it shows nothing.
- **Rolling digits**: values, uptime and battery percentage roll one digit at a
  time, in the direction the number moved.
- **Local staleness handling**: when no new data arrives for two seconds, the
  values and the uptime fall back to `-`, so a frozen PC never looks live.
- **Persistent link status**: the bottom-left corner always shows one of four
  Chinese status strings: no Wi-Fi, connected, connecting, or PC offline.
- **Settings page**: backlight brightness (10 % steps from 10 % to 100 %),
  Wi-Fi provisioning, screen rotation, and a device information page.
  Brightness and rotation are stored in NVS and restored at boot.
- **Wi-Fi provisioning over SoftAP**: start the `AIPassport-Setup` hotspot,
  scan nearby networks from a phone, pick an SSID, and enter the password and
  the PC's address. The configuration is stored in NVS, and the page itself is
  styled as a dark dot-matrix panel.
- **Terminal look**: pure black background, white text, and hard square corners.
  Text uses 24 px and 12 px subsets of the Fusion Pixel Font, stored in
  `assets/fonts/app_font_24.c` and `assets/fonts/app_font_12.c`. Both are pixel-exact
  because they are generated at multiples of the font's 12 px design grid; the source
  font and its OFL-1.1 licence are documented in
  [assets/README.md](assets/README.md#fonts).

## Interaction

The three keys are `UP`, `DOWN`, and `OK`.

**Monitor page**

- `OK` (long press): open the settings page. Short presses do nothing.

**Settings page**

- `UP` / `DOWN`: move the selection; the focused row shows a 2 px white frame.
- `OK`: enter the selected row.
- `OK` (long press): return to the monitor page.
- Brightness (the on-device row labels are Chinese): the frame turns green on
  entry; `UP` / `DOWN` change the value in 10 % steps and `OK` finishes.
- Rotation: `OK` flips the screen 180 degrees between the two landscape
  directions and saves the choice.
- Wi-Fi: `OK` opens a confirmation page; `OK` there starts provisioning and
  `UP` / `DOWN` cancels it.
- Device Info: read-only page with the configured PC address and the AIDA64
  sensor names to use.

**Setup mode** (entered after confirming Wi-Fi setup)

- Monitoring stops and the device raises the `AIPassport-Setup` hotspot.
- Open `http://192.168.4.1` on a phone, scan, and save the network.
- `OK` (long press): leave setup without saving.

## AIDA64 setup

1. In AIDA64, enable the RemoteSensor / LCD HTTP server and note its port
   (80 by default). The device always requests `http://<address>:<port>/sse`.
2. Add sensor items to the LCD list. Only the label keywords decide how an item is
   classified; AIDA64 supplies the value:

| Shown as | Label must also contain | Example |
| --- | --- | --- |
| CPU card | the device word `cpu` | `CPU Utilization` |
| GPU card | the device word `gpu` | `GPU Utilization` |
| Memory card | the device word `memory` or `ram` | `Memory Utilization` |
| Header uptime | `uptime` or `up time` | `UpTime` |

3. Utilization words: `util`, `usage`, `load`, `rate` and `occup`. A device word and
   a utilization word must both appear, or the record is ignored. Matching also
   accepts the corresponding Chinese terms for processor, graphics card, memory,
   occupancy, usage rate and usage, plus Chinese labels containing the words for
   uptime and boot time; the exact list is `classify_record()` in
   [`main/pc_metrics.c`](main/pc_metrics.c).
4. Value selection: the parser takes the last number of a record as the percentage,
   which tolerates labels carrying their own number such as `CPU #1 Utilization 12 %`,
   and clamps the result to 0-100.
5. Uptime values may be `1:23:45`, `0:12`, `1 day 2:03:04` or `3d 00:34:13`. The
   header shows the `xxD ` day prefix only when the raw value carries days.
6. A record whose display text exceeds 160 bytes is dropped entirely; other records
   in the same frame are unaffected, so keep labels short.

The parser reads the `data:` records of the `/sse` stream, splits them on `{|}`, and
treats each segment as `<element id>|<display text>`. Items matching no keyword are
ignored, so extra LCD items never disturb the display.

## Notes

- The panel is physically a 240 x 320 portrait part. The BSP swaps X and Y so
  this application runs at 320 x 240 landscape; use the Rotation row in the settings
  page if the image is upside down for your mounting.
- Utilization labels are matched in English and in some localized forms.
  English labels are the portable choice.
- Wi-Fi scanning briefly pauses the setup hotspot while the radio changes
  channels.
- The Wi-Fi scan on the setup page has not been re-confirmed on the device
  since its click-handler fix; every other behavior described here has been
  observed on hardware.
- Flashing the merged `full.bin` at `0x0` rewrites NVS and therefore erases the
  saved Wi-Fi configuration. Flashing only the application image at `0x10000`
  keeps it.

## Source

- Repository `KyRnln/ai-passport`, branch `feature/aida64-pc-monitor`.
- Built on the upstream FoloToy AI Passport baseline. See
  [docs/README.md](docs/README.md) for the hardware capability contract and
  [docs/development/ai-guide.md](docs/development/ai-guide.md) for the AI
  development workflow.
