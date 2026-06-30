# Headwater Backend — Handoff & Requirements

This is the **handoff list for the Headwater backend team**. The device-side Headwater
edition (v1 sync) is shipped and working, and the **Headwater App** (cross-day channel
browsing on the reader) is now **live on-device** — the per-summary anchors and per-issue
manifest it depends on have shipped from the backend and are verified end-to-end on X4
hardware. What remains is one backend feature (a rolling feed window) plus a deploy
confirmation; both are tracked below.

See [HEADWATER.md](HEADWATER.md) for the device-side design.

---

## Status (2026-06-29)

| Item | State |
|------|-------|
| **P1a** — per-summary anchors (`summary-<videoId>.xhtml`) | ✅ **Shipped.** Deep-link verified on-device. |
| **P1b** — embedded manifest (`OEBPS/headwater-manifest.json`) | ✅ **Shipped.** Channels view live + verified on X4. |
| **Rolling 14-day feed window** | ✅ **LIVE** (backend `6dde8ee`). Feed advertises the **last 14 completed digest-days**, newest-first, non-empty only — channel backlog is now 14 days deep. **Zero device work**; we already follow pagination + group across issues. Contract below. |
| **P0** — clean ISO title, no `dc:creator` | ⏳ **Pending prod redeploy.** Fix coded (`afd161d`, `28c9a10`). Feed `<title>` is already clean (`Headwater Daily YYYY-MM-DD`) via the rolling-feed deploy, so the **device filename is clean now**; the remaining `dc:creator`/`dc:title` cleanup is EPUB-internal and cosmetic to the device (we label from the filename). |
| **Named EPUB exports** | ⏳ **Committed, not pushed** (backend `8592ea7`, pending review). Manual "Download EPUB" sideload path takes an optional collection name → `dc:title = "Headwater — <Name>"`, filename `headwater-<name>.epub`. **By design these omit the manifest**, so they land in My Summaries (flat sideload), **not** Channels. See the open question below. |
| **Manifest in named exports** | ➡️ **Requested (2026-06-29).** Device is ready — Channels scans `/Headwater/My Summaries/` for manifests. Ask: emit the **same** `OEBPS/headwater-manifest.json` in export EPUBs, listing that export's videos (same per-item schema + matching anchors as the digest). Then saved collections merge into Channels with no device change. Manifest-less exports keep working (My Summaries only). See "Manifest in named exports" below. |
| **P2** — AccountPage token + copy-URL onboarding | Open (see below). |

### Rolling-feed contract (device-relevant, backend `6dde8ee`)
- Entry `<title>` = `Headwater Daily YYYY-MM-DD` (ISO, date-led) — unique per issue, so filename dedup/sort holds.
- Entry `<id>` + acquisition href = stable dated URL `…/opds/<token>/<date>.epub`.
- `<updated>` = issue close timestamp (11:00 UTC of that day), **stable across fetches** (not request time).
- Closed issues are **immutable** (`Cache-Control: max-age=86400`) → a dated URL always returns the same bytes; safe to cache device-side.
- Day boundary = **11:00 UTC for everyone** (matches the timezone-less digest). Near-rollover email-vs-device drift is known/accepted.

### Manifest in named exports — the ask (2026-06-29)
**What:** when generating a named "Download EPUB" collection (`8592ea7`), also write the **same**
`OEBPS/headwater-manifest.json` you already emit for daily digests — just scoped to the videos in
that export instead of the day's set. No new schema, no new endpoint, same `epub.js` path.

**Why:** the device's Channels view is built **only** from embedded manifests, and it now scans both
`/Headwater/` and `/Headwater/My Summaries/` (where sideloaded exports land). With a manifest, each
video in a saved collection merges into its channel in Channels automatically — **zero device change.**

**Shape** (identical to the digest manifest, see §3):
- `items[]` = one entry **per video in the export**, each `{ channelId, channel, videoId, videoTitle, anchor, date }`.
- `anchor` must exactly match that video's TOC target in the export EPUB (reusing the digest's
  `summary-<videoId>.xhtml` scheme makes this automatic).
- `issue` block: `{ id, title, date }` — for an export, `id`/`title` can be the collection name; `date` the export date. (Device groups on `channelId`, so the issue block is informational here.)

**Degradation:** an export **without** a manifest still works — it just shows under My Summaries and is
absent from Channels. So this is purely additive; nothing breaks if it ships later or never.

Manifest `issue.title` is the clean display source of truth (independent of `dc:title`);
`issue.id` / `issue.date` are bare ISO. The device reads its issue-list label from the
(now-clean) filename, so no manifest-title read is needed there.

No device-side retention/pruning is built or planned at current feed scale — and any future
pruning must be **manifest-aware**, since `/Headwater` also holds sideloaded user exports
(no manifest) that must never be deleted.

---

## TL;DR — original contract (for history)

1. **Unique, date-stamped issue titles** in the OPDS feed (idempotency). — *P0, fixed in code, see Status.*
2. **One TOC/chapter entry per video summary** with a stable anchor. — *P1a, shipped.*
3. **A per-issue manifest** describing each summary: `{channel, videoTitle, anchor, date, new}`. — *P1b, shipped.*

---

## Hardware-verified notes (2026-06-28)

One-press sync is shipped and **confirmed end-to-end on real X4 hardware**: the device connects
Wi-Fi, pulls `https://headwaterapp.com/opds/<token>`, downloads new issues into `/Headwater`, and
returns to an on-device Headwater app that lists them. Auth + feed + download all work. What we
still need from the feed / EPUB generation, in priority order:

**P0 — Clean, unique filenames (do first).** Idempotency is filename-based; the device skips an
entry if `sanitizeFilename(author ? author + " - " + title : title) + ".epub"` already exists.
Verified the current feed yields a **doubled** name: `author="Headwater"` +
`title="Headwater — June 27, 2026"` → `Headwater - Headwater — June 27, 2026.epub`. Pick
`author`/`title` together so the stem is one clean, unique, date-stamped string, e.g.
`Headwater Daily — 2026-06-27.epub` (simplest: `author=""`, `title="Headwater Daily — 2026-06-27"`).
Feed `<title>` and EPUB `dc:title` move together. Two issues sharing a title → the second is
silently skipped. *(See §1.)*

**P1a — Stable per-summary anchors.** Verified the current EPUBs use index-based docs
(`OEBPS/ch001.xhtml`), which move between rebuilds and break deep-linking. Switch to video-id-based
`summary-<videoId>.xhtml` (OPF id `v-<videoId>`), one TOC entry per summary. Device deep-links by
spine index + anchor — no reader changes needed. *(See §2.)*

**P1b — Embedded per-issue manifest** at `OEBPS/headwater-manifest.json` (Option A). The device
builds the cross-day channels index from these; `anchor` must exactly match the EPUB TOC target.
*(Schema in §3.)*

**P2 — AccountPage token + feed-URL copy button.** Enables zero-typing onboarding (copy URL → paste
into the reader's web settings). Keep the URL on the `headwaterapp.com` host (device auto-detects by
that host); no QR-to-scan (device has no camera); warn that regenerating the token invalidates the
saved feed. *(See "OPDS token onboarding".)*

**Settled, no action:** Saved videos = device-local sideload (USB/file-transfer), no synced shelf.

---

## 1. Unique, date-stamped issue titles  (priority: now)

The device decides "do I already have this issue?" purely by filename, derived from the OPDS
`<title>`: it skips download if `sanitizeFilename(author + " - " + title).epub` already exists
on the SD card.

**Requirement:** every daily issue must have a **globally unique title**, ideally date-stamped,
e.g. `Headwater Daily — 2026-06-27`. If two different issues ever share a title, the second is
silently treated as "already downloaded" and skipped.

This is the only hard requirement for v1 to be robust. Confirm the current feed already does this.

---

## 2. EPUB authoring — one chapter per summary  (priority: gates the app)

The reader deep-links by **spine index + anchor** (verified: `Epub::getTocItem(i)` →
`{spineIndex, anchor, title, level}`; the reader navigates to exactly that). For the channels
view to "open the digest at a specific summary," each summary must be addressable.

**Requirement:** in each daily-digest EPUB, author **each video summary as its own TOC entry**
(a chapter / nav point) with a **stable anchor** — either its own spine document
(`summary-<videoId>.xhtml`) or a fragment id (`<section id="v-<videoId>">`) referenced from the
EPUB nav/TOC.

- The anchor must be **stable across rebuilds** of the same issue (use the YouTube video id, not a hash of content).
- The anchor in the manifest (item 3) must exactly match what the EPUB's nav/TOC points to, so the device can resolve manifest entry → TOC entry → spine position.

This keeps the device's reader engine untouched — we reuse the existing chapter-jump path.

---

## 3. Per-issue manifest  (priority: gates the app)

The device can read an EPUB's TOC, but the TOC alone doesn't carry **channel grouping**,
**publish date per video**, or **"new since last sync"** state. The manifest supplies that
structured metadata so the device never has to parse titles or guess.

### Schema

One manifest **per issue**, listing every summary in that issue:

```json
{
  "issue": {
    "id": "2026-06-27",
    "title": "Headwater Daily — 2026-06-27",
    "date": "2026-06-27"
  },
  "items": [
    {
      "channelId": "UCHnyfMqiRRG1u-2MsSQLbXA",
      "channel": "Veritasium",
      "videoTitle": "The Surprising Physics of ...",
      "videoId": "abc123",
      "anchor": "summary-abc123.xhtml",
      "date": "2026-06-26",
      "new": true
    }
  ]
}
```

Field notes:
- `channelId` — the YouTube `UC…` id; **the stable grouping key** the device indexes on. (Confirmed available: `summaries.channel_id`.)
- `channel` — display name only; safe to vary spelling/casing since grouping keys on `channelId`.
- `videoTitle` — shown in the channels list.
- `anchor` — **must match** the EPUB TOC target for this summary (see item 2). Spine path and/or `#fragment`.
- `date` — the video's publish date (or summary date); used for sorting within a channel.
- `new` — true if this is newly added in this issue vs. the prior one. (Device also infers "new since last sync" itself, so this is a hint, not load-bearing — a cheap "saved within last 24h" heuristic is fine; no extra query needed.)
- `videoTitle` — shown in the channels list.
- `anchor` — **must match** the EPUB TOC target for this summary (see item 2). Spine path and/or `#fragment`.
- `date` — the video's publish date (or summary date); used for sorting within a channel.
- `new` — true if this is newly added in this issue vs. the prior one. (Device also infers "new since last sync" itself, so this is a hint, not load-bearing.)

### Delivery mechanism — pick one (backend's choice; device adapts)

The current OPDS parser only captures `title / author / href / id` (no custom link rels), so
the simplest options are:

- **Option A (recommended): embed the manifest inside the EPUB** at a fixed path, e.g.
  `OEBPS/headwater-manifest.json` or `META-INF/headwater.json`. It travels atomically with the
  issue (no second request, no sync mismatch), and the device already unzips EPUBs. *(Device-side
  TODO: confirm the EPUB lib can extract an arbitrary entry by path.)*
- **Option B: sidecar URL by convention** — same URL as the acquisition link with a `.json`
  extension, or a dedicated `…/manifest/<issueId>.json` endpoint. Requires the device to fetch a
  second file per issue and a firm URL convention.
- **Option C: custom OPDS `<link rel="…/headwater-manifest" href="…">`** on each acquisition
  entry. Cleanest semantically but requires extending the device's OPDS parser to capture the rel.

Recommendation: **Option A** unless there's a reason the manifest must change without
re-issuing the EPUB.

---

## Resolved with the backend team (2026-06-27)

1. **Unique date-stamped titles — already done.** The feed already emits one unique entry per
   day (`…/2026-06-27.epub`, matching `dc:title`). **Action:** switch the title wording to ISO
   `Headwater Daily — 2026-06-27`, *and* set the OPDS `<author>` so the device's
   `author + " - " + title` filename derivation produces a clean, un-doubled name. (Today
   `author="Headwater"` + `title="Headwater — June 27, 2026"` yields
   `Headwater - Headwater — June 27, 2026.epub`.) Pick author/title together so the stem is just
   the ISO issue name — e.g. omit the author, or move the brand into the author and keep the
   title date-only. Feed `<title>` and EPUB `dc:title` must move together.

2. **Per-summary TOC — TOC done, anchors need the fix.** The generator already emits one XHTML +
   OPF spine item + NCX navPoint per summary. The only change: anchors are currently index-based
   (`ch001.xhtml`), so a video can move between rebuilds. Switch to video-id-derived
   `summary-<videoId>.xhtml` with OPF id `v-<videoId>` (prefixed — XML ids can't start with a
   digit). Then manifest `anchor` == spine filename. No reader-engine change.

3. **Manifest delivery = Option A (embed in EPUB).** `OEBPS/headwater-manifest.json`, one
   `zip.file()` call. All fields available from existing summary rows.

4. **Stable `channelId` — yes.** `summaries.channel_id` (the YouTube `UC…` id) exists alongside
   `channel_name`. Manifest carries both: `channelId` as the grouping key, `channel` as display.
   *(Device note: the index groups on `channelId`; `channel` is display-only.)*

5. **Saved videos — DEVICE-LOCAL SIDELOAD, no synced shelf. (decided)** The feed delivers
   **daily digests only**. User-saved videos are a manual "select → export EPUB" the user
   sideloads (USB / file transfer) — not a synced acquisition entry. This matches the locked
   "custom-export EPUBs are user-driven" decision; the persistent synced "Saved" shelf was
   explicitly rejected (too costly for a 380 KB device).

   **Device model (shipped):** sideloaded exports go in a dedicated **`/Headwater/My Summaries/`**
   subfolder, surfaced on its own "My Summaries" page in the Headwater app (Today's digest stays
   the main inbox; older digests live under "Archived"). The directory scan skips subfolders, so
   sideloads never pollute the daily inbox or the Archived list.

   **➤ Backend ask (small, optional but wanted): embed the manifest in per-summary exports too.**
   Channels is built **only from `OEBPS/headwater-manifest.json`**, and the device now scans both
   `/Headwater/` *and* `/Headwater/My Summaries/` for manifests. So if the per-summary "export
   EPUB" embeds the **same manifest format** (just a single `items[]` entry: `channelId`,
   `channel`, `videoTitle`, `anchor`, `date`), each saved summary automatically merges into its
   channel in Channels alongside the digests — no new format, no new endpoint. A per-summary
   export **without** a manifest still works: it just appears under "My Summaries" and is absent
   from Channels (graceful degradation). Deleting a saved summary on-device also removes it from
   Channels (same self-heal as digests), and the delete confirmation warns about this.

---

## OPDS token onboarding — resolved (2026-06-27)

**Backend action: build an AccountPage section for token + feed URL.**

The device already has a full OPDS server management UI at its own web page (device web server
`/api/opds` GET/POST/DELETE + editable "Add OPDS server" section). That means the user-facing
onboarding flow is **copy + paste, no typing on the reader**:

1. AccountPage: user generates token → sees full feed URL → **Copy** button.
2. User opens the **device's File Transfer web page** (browser on the same Wi-Fi) → pastes
   the URL into "Add OPDS server."
3. Done. The device auto-detects it as Headwater and "Check Headwater" lights up.

**What AccountPage must own:**
- Generate / view / **regenerate (revoke)** the OPDS token.
- The full feed URL + copy button.
- Instructions: "Copy the URL above, then open your reader's web page and paste it into Add OPDS Server."

**Three device constraints to honor:**

1. **Keep the feed URL on `headwaterapp.com`** (or a subdomain). The device auto-identifies the
   Headwater feed by substring-matching `headwaterapp.com` in the stored URL
   (`getHeadwaterServer()`). A subdomain like `api.headwaterapp.com` still matches; a different
   apex domain would not auto-detect and the sync entry would not appear.

2. **No camera on the device — it cannot scan a QR.** Do not design a "scan this QR with your
   reader" step. (A QR on AccountPage can help a phone scan it, but that doesn't get the URL
   onto the reader.) Copy-to-clipboard + paste into device web UI is the correct path. (The
   device can *display* a QR, relevant only for a future pairing flow — it cannot read one.)

3. **Token regeneration changes the URL → the device's saved server goes stale.** Sync fails
   gracefully (no crash), but the user must re-paste the new URL. Warn on the regenerate button.

**Future (v2, non-blocking):** true zero-typing pairing = device displays a short code → user
enters it on the web → backend binds feed to device. The token model built now stays valid
underneath; no re-architecture needed.

---

## What the device will do with this (for context)

On each sync, the device merges every issue's manifest into a rolling **cross-issue index** on
the SD card: `channel -> [{ digestFile, anchor, date, videoTitle, read? }]`. The Headwater App
then renders a channels view from that index; selecting an item opens the owning digest EPUB at
the anchor, and on Back auto-advances to the next unread item. Read state is maintained
device-side. Old issues (and their index entries) are pruned by a retention window.
