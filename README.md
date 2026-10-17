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
