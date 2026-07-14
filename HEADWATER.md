# Headwater Edition

An **additive fork of CrossPoint** that turns an Xteink X3/X4 into a one-press reader for
[Headwater](https://headwaterapp.com) — a service that condenses the YouTube channels you follow into a
concise daily digest of written summaries. Headwater already exposes a per-user **OPDS catalog + EPUB
generation**; this edition removes the last manual step: getting each new issue onto the reader and giving
it a home there.

Everything in upstream CrossPoint is unchanged — this edition only **adds** a dedicated Headwater app on
top, so upstream releases can be re-merged. Licensed MIT; based on
[CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader) by Dave Allie.

## What it adds

A **Headwater** entry on the Home screen (pre-selected at boot) opens `HeadwaterAppActivity`, a masthead
menu whose rows adapt to what's on the device:

- **Update** — one press connects Wi-Fi from saved credentials, pulls any new daily issue over OPDS, and
  silent-restarts back into the app with today's issue ready. Headless; user-triggered only.
- **Today** — opens the newest issue directly.
- **Channels** — cross-day browsing of every summary grouped by its YouTube channel, built from an
  embedded per-issue manifest. Read tracking (unread dots), long-press curation (mark read, hide a
  summary, mute a channel), and a recency cap keep it tidy.
- **My Summaries** — collections sent to the device from Headwater (or sideloaded), which also merge into
  Channels via their manifest.
- **Archived** — older daily issues, out of the main inbox.

First run is self-guiding: on a blank SD card with no feed configured, the app opens to a **"Connect your
account"** screen that walks the user through pairing (below), so a freshly-flashed device is never a dead
end.

## How it works

**Sync.** `OpdsSyncActivity` connects Wi-Fi, walks the OPDS feed (following pagination), and atomically
downloads any issue not already present. A feed entry whose acquisition href contains `/export/` is a
sent-to-device collection and downloads into `/Headwater/My Summaries/`; everything else is a daily issue
in `/Headwater/`. Wi-Fi is torn down and the device silent-restarts afterward to clear the post-TLS heap
fragmentation. The whole flow is skippable (Back, including mid-download) and bounded by a connect timeout
and an overall budget.

**Channels index.** Each issue and export EPUB carries a hidden `OEBPS/headwater-manifest.json` listing its
summaries — `{ channelId, channel, videoId, videoTitle, anchor, date }`. `buildChannelIndex()` scans
`/Headwater` and `/Headwater/My Summaries`, parses each manifest, and groups items by `channelId` (the
stable YouTube `UC…` id; `channel` is display-only). Selecting a summary deep-links the reader into the
owning EPUB at that anchor — the reader engine is untouched. Videos are de-duplicated by `videoId`, so one
that appears in both a daily issue and a sent export lists once.

**Curation state.** Read / hidden / muted state is persisted as newline-separated id lists in hidden SD
sidecars (`/Headwater/.read`, `.hidden`, `.muted`), keyed by `videoId`/`channelId` so state survives
re-sync and export. Deleting a daily issue writes a filename tombstone (`/Headwater/.deleted`) so the
rolling feed doesn't re-download it. None of these are `.epub`, so the issue/channel scans skip them.

**Navigation.** Opening a summary from Channels records an in-memory return hint; the reader's Back routes
to the Headwater app, which re-opens Channels at that channel on the next unread — so you read down a
channel without bouncing to the menu. The hint clears on reaching Home.

## Design decisions

- **One feed, token-keyed.** The backend serves a single per-user OPDS root; the daily digest and sent
  collections both arrive as acquisition entries. The device downloads what's new and routes by href — no
  second endpoint, no synced "shelf" to reconcile.
- **Feed identity by host.** The Headwater feed is the first saved server in `OPDS_STORE` whose URL host
  contains `headwaterapp.com` (`getHeadwaterServer()`); the per-user token is embedded in that URL, pasted
  once on-device. No new storage or schema.
- **Atomic downloads.** Issues download to `<name>.epub.part` and rename to `.epub` only on completion, so
  an aborted download never leaves a partial file in the library.
- **Idempotency by filename.** A feed entry is skipped if its target filename already exists, so each issue
  must carry a globally unique (date-stamped) title — a backend contract. Exports carry their unique export
  id in the title for the same reason.
- **`channelId` is the grouping key.** Channels groups on the stable `UC…` id, not the display name; items
  with a blank `channelId` are dropped (they degrade to My-Summaries-only), so the backend must populate it
  on every manifest item.
- **Theme-portable unread mark.** Of the per-row `drawList` hooks, only the right-aligned *value* slot is
  rendered by every theme (RoundedRaff ignores `rowDimmed`/`rowIcon`), so the unread dot rides there.
- **No network OTA.** CrossPoint's OTA pulls from upstream's GitHub releases and would overwrite this fork
  with stock CrossPoint. It's removed entirely; firmware updates go via SD-card flashing only. The build
  version is tagged `1.4.0+headwater` for the same reason — to never masquerade as an upstream build.
- **Why the X4 can't auto-sync.** On battery, deep sleep is a full power-off via a GPIO13 latch MOSFET
  (`lib/hal/HalPowerManager.cpp`): the MCU, internal RTC, and RTC memory are all lost, and the only battery
  wake is the power button. The X4 keeps no wall clock across sleep, so an offline "is it a new day?" gate
  is impossible there. Only the X3's coin-cell-backed DS3231 keeps real time — hence sync is one-press on
  both, and any future auto-on-wake is X3-only.

## Key modules

- `src/activities/network/OpdsSyncActivity.{h,cpp}` — the headless sync + `/export/` routing.
- `src/activities/headwater/` — the app (`HeadwaterAppActivity`), Channels browser
  (`HeadwaterChannelsActivity`), folder browser for Archived/My Summaries (`HeadwaterFolderActivity`),
  manifest parsing (`HeadwaterManifest`), channel index (`HeadwaterChannelIndex`), curation-state store
  (`HeadwaterIdSet`), deleted-issue tombstones (`HeadwaterDeleted`), return-nav hint (`HeadwaterNav`), and
  SD paths (`HeadwaterPaths`).
- `src/OpdsServerStore.{h,cpp}` — `getHeadwaterServer()` host match.
- `src/network/HttpDownloader.{h,cpp}` — `downloadToFileAtomic()`.
- `src/activities/home/HomeActivity.cpp` — always-present Headwater entry, pre-selected at boot.
- `src/images/HeadwaterHeader.h` — the app masthead (NotoSerif wordmark + rule, baked 1-bit).
- `src/images/HeadwaterEdition.h` — the "Headwater Edition" boot wordmark.
- `src/activities/boot_sleep/{BootActivity,SleepActivity}.cpp` — branding + MIT attribution line.

## Build

```bash
pio run -e default
```

Requires PlatformIO (pioarduino); see the [README](README.md#build-from-source) for setup. A pre-built
`.bin` is distributed on the Headwater site; it can also be built from source here.

## Pairing (no typing on the reader)

The per-user feed URL is added via the reader's web interface, so nothing is typed on the device:

1. On the reader: join Wi-Fi and open **File Transfer** to start its web server; it shows a web address.
2. On a computer on the same Wi-Fi: open that address, then **Settings → OPDS servers → Add**. Paste the
   Headwater feed URL from your account page; leave username/password blank; Save.

The server persists to the SD card and survives reboots — done once. The Headwater app auto-detects it by
host and **Update** works immediately.

## Roadmap

- **X3 automatic on-wake sync** — an epoch-gated auto-sync in `setup()` using the X3's DS3231 RTC (X4 stays
  one-press by hardware necessity, above).
- **Merge upstream CrossPoint 1.4.1+** — periodic re-merge of upstream releases onto the fork.
