# Headwater Edition

*An MIT-licensed fork of [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) that turns an
Xteink X3/X4 into a one-press reader for [Headwater](https://headwaterapp.com).*

Headwater condenses the YouTube channels you follow into a concise daily digest — written summaries you
read instead of watch. Headwater Edition puts that digest on your e-ink reader: power on, open Headwater,
press **Update**, and today's issue downloads and is ready to read. No phone, no feed, no notifications.

Everything CrossPoint already does — EPUB reading, file browser, OPDS, wireless transfer, themes, custom
fonts — still works underneath. This edition only **adds** a dedicated Headwater app on top.

## What it adds

- **One-press Update** — a headless Wi-Fi sync that pulls your daily digest onto the reader.
- **Channels** — browse two weeks of summaries grouped by the channels you follow, with read tracking
  (unread dots), and long-press curation to hide a summary or mute a channel.
- **My Summaries** — a home for collections you send to the device from Headwater.
- **A tidy inbox** — delete an issue and it stays deleted; older issues roll into Archived. First run
  self-guides through pairing.

See **[HEADWATER.md](./HEADWATER.md)** for the device-side design and architecture.

## Built on CrossPoint

The base firmware — the reader engine, format support, custom fonts, themes, web transfer, and OPDS — is
all [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader). For the full base feature list,
internals, and contributor docs, see the upstream README. This fork tracks CrossPoint and re-merges its
releases.

## ⚠️ Before you flash

Headwater Edition **removes CrossPoint's network OTA updater** — deliberately, so an upstream release can't
silently overwrite this fork. Firmware is updated by re-flashing over USB or via the built-in **SD-card
firmware update** (which is retained, and is your recovery path back to stock CrossPoint).

**USB-locked devices:** some Xteink units (often from third-party stores) ship with USB flashing locked. If
you bought directly from xteink.com, your device is not locked and you're fine. If you're unsure, confirm
the reader shows up in your browser's serial picker (i.e. it flashes over USB) **before** flashing anything.
See [locked-device notes](https://crosspointreader.com/#unlock-tool).

## Install

1. Download the Headwater Edition `firmware.bin` (from the [Headwater site](https://headwaterapp.com) or
   this repo's Releases).
2. Connect the reader via USB-C and flash it — CrossPoint's web flasher (**Custom .bin**) at
   <https://crosspointreader.com/#flash-tools>, or `esptool`:
   ```bash
   esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 firmware.bin
   ```
3. To return to stock CrossPoint, flash an official release from the same tools.

## Build from source

```bash
git clone --recursive <this-repo-url>
cd headwater-edition
pio run -e default          # build
pio run --target upload     # build + flash
```

Requires PlatformIO (pioarduino). See the upstream
[Development quick start](https://github.com/crosspoint-reader/crosspoint-reader#development-quick-start)
for prerequisites and pre-PR checks.

## Pair your account (no typing on the reader)

1. On the reader: join Wi-Fi, then open **File Transfer** to start its web server — it shows a web address.
2. On a computer on the same Wi-Fi: open that address, then go to **Settings → OPDS servers → Add**. Paste
   your Headwater feed URL from your account page; leave username and password blank; Save.

The feed saves to the device and survives reboots — you only do this once. Then press **Update**.

## License & attribution

Headwater Edition is an MIT-licensed fork of **CrossPoint** by Dave Allie. © 2025 Dave Allie. See
[LICENSE](./LICENSE).

Not affiliated with Xteink, or with the upstream CrossPoint project.
