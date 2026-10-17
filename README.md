<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# AIDA64 PC Monitor

Turn the AI Passport into a wearable status display for a Windows PC. The
device joins your Wi-Fi, connects to AIDA64's RemoteSensor stream, and shows
live CPU, GPU, and memory utilization plus the PC's uptime on a 320 x 240
landscape screen in a black-and-green terminal style.

This branch (`feature/aida64-pc-monitor`) contains the application only. The
upstream baseline, hardware contract, and documentation index stay in
[docs/README.md](docs/README.md).

## What it does

- **Utilization cards**: CPU, GPU, and memory percentage, each drawn as a
  value line plus a full-width bar. A bar turns red at 100 %.
- **PC uptime in the header**: read from AIDA64's uptime sensor and shown as
  `h:mm:ss` next to the battery percentage.
- **Local staleness handling**: when no new data arrives for two seconds, the
  values and the uptime fall back to `-`, so a frozen PC never looks live.
- **Persistent link status**: the bottom-left corner always shows one of
  `No Wi-Fi`, `Connected`, `Connecting`, or `PC offline`.
- **Settings page**: backlight brightness (10 % steps from 10 % to 100 %),
  Wi-Fi provisioning, screen rotation, and a device information page.
  Brightness and rotation are stored in NVS and restored at boot.
- **Wi-Fi provisioning over SoftAP**: start the `AIPassport-Setup` hotspot,
  scan nearby networks from a phone, pick an SSID, and enter the password and
  the PC's address. The configuration is stored in NVS.
- **Terminal look**: pure black background, white text, and hard square corners.
  Text uses the subset of the Hanyi pixel face `HYPixel 11px U` stored in
  `assets/fonts/app_font_26.c` and `assets/fonts/app_font_13.c`. Read the
  [licence status of those generated fonts](assets/README.md#licence-status-of-the-generated-fonts)
  before publishing or distributing this project.

## Interaction

The three keys are `UP`, `DOWN`, and `OK`.

**Monitor page**

- `OK` (long press): open the settings page. Short presses do nothing.

**Settings page**

- `UP` / `DOWN`: move the selection; the focused row shows a thin border.
- `OK`: enter the selected row.
- `OK` (long press): return to the monitor page.
- `Brightness`: `UP` / `DOWN` change the value in 10 % steps while the row
  shows a thick bright border; `OK` finishes.
- `Rotation`: `OK` flips the screen 180 degrees between the two landscape
  directions and saves the choice.
- `Wi-Fi`: `OK` opens a confirmation page; `OK` there starts provisioning and
  `UP` / `DOWN` cancels it.
- `Device Info`: read-only page with the configured PC address and the AIDA64
  sensor names to use.

**Setup mode** (entered after confirming Wi-Fi setup)

- Monitoring stops and the device raises the `AIPassport-Setup` hotspot.
- Open `http://192.168.4.1` on a phone, scan, and save the network.
- `OK` (long press): leave setup without saving.

## AIDA64 setup

1. In AIDA64, enable the RemoteSensor / LCD HTTP server and note its port
   (80 by default).
2. Add simple sensor items whose labels let the parser classify them:
   - `CPU` plus a utilization word, for example `CPU Utilization`
   - `GPU` plus a utilization word
   - `Memory` (or `RAM`) plus a utilization word
   - an uptime item whose label contains `UpTime`
3. Enter the PC's address and port on the device's setup page.

The parser reads the `data:` records of the `/sse` stream, splits them on
`{|}`, and takes the last number of a record as its percentage.

## Notes

- The panel is physically a 240 x 320 portrait part. The BSP swaps X and Y so
  this application runs at 320 x 240 landscape; use `Rotation` if the image is
  upside down for your mounting.
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
