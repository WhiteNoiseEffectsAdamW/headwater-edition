#include "OpdsSyncActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <OpdsStream.h>
#include <WiFi.h>

#include <set>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "WifiCredentialStore.h"
#include "activities/headwater/HeadwaterDeleted.h"
#include "activities/headwater/HeadwaterPaths.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"
#include "util/BookCacheUtils.h"
#include "util/StringUtils.h"
#include "util/UrlUtils.h"

namespace {
// Time to wait for the Wi-Fi association before giving up.
constexpr unsigned long CONNECT_TIMEOUT_MS = 20000;
// Hard ceiling on the whole sync so a stalled feed/connect can't strand the
// user on the "Checking…" screen. Mid-download is bounded separately by the
// HTTP socket timeout and the skip poll.
constexpr unsigned long SYNC_BUDGET_MS = 120000;
// Guard against a malformed feed advertising an unbounded pagination chain.
constexpr int MAX_FEED_PAGES = 20;
// How long the completion message ("You're up to date") stays visible before the
// silent restart, so a nothing-new sync gives feedback instead of a blink-and-gone
// bounce to Home. A button press skips the wait.
constexpr unsigned long DONE_HOLD_MS = 1800;
}  // namespace

void OpdsSyncActivity::onEnter() {
  Activity::onEnter();

  state = State::CONNECTING;
  pending.clear();
  currentIssue = 0;
  downloadedCount = 0;
  cancelRequested = false;
  errorMessage.clear();
  statusMessage = tr(STR_HEADWATER_CHECKING);
  syncStartMs = millis();
  requestUpdate();

  // WIFI_STORE isn't loaded at boot; pull saved credentials now. SD access is
  // serialized, but lock out the render task while we touch the shared SPI bus.
  {
    RenderLock lock(*this);
    WIFI_STORE.loadFromFile();
  }

  startConnect();
}

void OpdsSyncActivity::onExit() {
  Activity::onExit();
  pending.clear();

  // If we brought Wi-Fi up, tear it down and silently reboot to clear the heap
  // fragmentation a Wi-Fi/TLS session leaves behind (same as the OPDS browser).
  // When no credentials were available we never touched Wi-Fi, so just exit.
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(true);
    delay(30);
    silentRestartToHeadwater();  // land back in the Headwater app, not Home
  }
}

void OpdsSyncActivity::startConnect() {
  // Already connected (Wi-Fi left up by a prior flow)? Go straight to fetching.
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    state = State::FETCHING;
    return;
  }

  const std::string ssid = WIFI_STORE.getLastConnectedSsid();
  const WifiCredential* cred = ssid.empty() ? nullptr : WIFI_STORE.findCredential(ssid);
  if (!cred) {
    state = State::ERROR;
    errorMessage = tr(STR_HEADWATER_NO_WIFI);
    requestUpdate();
    return;
  }

  WiFi.persistent(false);  // Credentials are managed by WifiCredentialStore; suppress SDK NVS auto-connect
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

  String mac = WiFi.macAddress();
  mac.replace(":", "");
  const String hostname = "CrossPoint-Reader-" + mac;
  WiFi.setHostname(hostname.c_str());

  if (!cred->password.empty()) {
    WiFi.begin(cred->ssid.c_str(), cred->password.c_str());
  } else {
    WiFi.begin(cred->ssid.c_str());
  }
  connectStartMs = millis();
}

void OpdsSyncActivity::fetchAndQueue() {
  if (server.url.empty()) {
    state = State::ERROR;
    errorMessage = tr(STR_HEADWATER_SYNC_FAILED);
    requestUpdate();
    return;
  }

  Storage.mkdir(headwater::ISSUES_DIR);  // ensure the destination folder exists (no-op if present)
  // Pushed "send-to-device" collections land in My Summaries (kept out of the
  // daily inbox); ensure that folder exists before any export download.
  Storage.ensureDirectoryExists(headwater::MY_SUMMARIES_DIR);

  // Issues the user deleted on-device: don't re-pull them while they're still in
  // the feed window. Track every filename the feed offers so the tombstone list
  // can be pruned back to the current window once the walk succeeds.
  const std::set<std::string> deleted = headwater::loadDeletedIssues();
  std::set<std::string> feedNames;

  // Walk the feed (following pagination) and queue any issue not already on disk.
  std::string url = server.url;
  for (int page = 0; !url.empty() && page < MAX_FEED_PAGES; ++page) {
    OpdsParser parser;
    {
      OpdsParserStream stream{parser};
      if (!HttpDownloader::fetchUrl(url, stream, server.username, server.password) || !parser) {
        state = State::ERROR;
        errorMessage = tr(STR_HEADWATER_SYNC_FAILED);
        requestUpdate();
        return;
      }
    }

    for (const auto& entry : parser.getEntries()) {
      if (entry.type != OpdsEntryType::BOOK) continue;
      // Resolve the download URL relative to the page it came from.
      const std::string downloadUrl = UrlUtils::buildUrl(url, entry.href);
      // Pushed collections are served from /opds/<token>/export/<id>.epub; route
      // those into My Summaries so they don't pollute the daily inbox / Archived.
      // Detection is on the href (stable contract), not the title (display-only).
      const bool isExport = entry.href.find("/export/") != std::string::npos;
      const char* destDir = isExport ? headwater::MY_SUMMARIES_DIR : headwater::ISSUES_DIR;
      const std::string fileName =
          StringUtils::sanitizeFilename((entry.author.empty() ? "" : entry.author + " - ") + entry.title) + ".epub";
      feedNames.insert(fileName);
      if (deleted.count(fileName)) continue;  // user deleted it; don't re-pull
      const std::string path = std::string(destDir) + "/" + fileName;
      if (Storage.exists(path.c_str())) continue;  // idempotent: we already have this issue
      pending.push_back({downloadUrl, path, entry.title});
    }

    // Advance to the next page, resolving relative links against the current page.
    const std::string& next = parser.getNextPageUrl();
    url = next.empty() ? "" : (next.rfind("http", 0) == 0 ? next : UrlUtils::buildUrl(url, next));
  }

  // The whole feed walked cleanly (errors return early above). Prune tombstones
  // for issues that have aged out of the window so the list can't grow forever.
  if (!deleted.empty()) {
    std::set<std::string> kept;
    for (const auto& name : deleted) {
      if (feedNames.count(name)) kept.insert(name);
    }
    if (kept.size() != deleted.size()) headwater::saveDeletedIssues(kept);
  }

  currentIssue = 0;
  if (pending.empty()) {
    state = State::DONE;
    statusMessage = tr(STR_HEADWATER_UP_TO_DATE);
    doneAtMs = millis();  // hold the message briefly (loop()'s DONE case restarts)
    requestUpdate();
  } else {
    state = State::DOWNLOADING;
    requestUpdate();
  }
}

void OpdsSyncActivity::downloadNext() {
  const PendingIssue& issue = pending[currentIssue];
  statusMessage = issue.title;
  downloadProgress = downloadTotal = 0;
  requestUpdate(true);
  LOG_DBG("HWSYNC", "Downloading: %s -> %s", issue.url.c_str(), issue.path.c_str());

  const auto result = HttpDownloader::downloadToFileAtomic(
      issue.url, issue.path,
      [this](const size_t downloaded, const size_t total) {
        downloadProgress = downloaded;
        downloadTotal = total;
        requestUpdate(true);
        // The main loop is blocked during this download, so poll input directly
        // to honour a mid-download skip; the read loop checks cancelRequested.
        mappedInput.update();
        if (mappedInput.wasReleased(MappedInputManager::Button::Back)) cancelRequested = true;
      },
      &cancelRequested, server.username, server.password);

  if (result == HttpDownloader::ABORTED) {
    finishToApp();
    return;
  }
  if (result == HttpDownloader::OK) {
    clearBookCache(issue.path);
    downloadedCount++;
  } else {
    // A single failed issue shouldn't abort the batch; skip it and continue.
    LOG_ERR("HWSYNC", "Download failed (%d): %s", result, issue.path.c_str());
  }
  currentIssue++;
}

void OpdsSyncActivity::finishToApp() {
  // Return to the Headwater app where the sync was launched. If Wi-Fi was
  // brought up, onExit() reboots via silentRestartToHeadwater() (to clear heap
  // fragmentation) and that lands on the app; if Wi-Fi never came up (e.g. no
  // saved credentials), no reboot is needed and this navigates there directly.
  activityManager.goToHeadwaterApp();
}

bool OpdsSyncActivity::checkSkip() {
  return cancelRequested || mappedInput.wasReleased(MappedInputManager::Button::Back);
}

void OpdsSyncActivity::loop() {
  // Whole-sync time budget (does not fire mid-download; that's bounded by the
  // HTTP socket timeout and the skip poll inside downloadNext()).
  if (state != State::ERROR && state != State::DONE && millis() - syncStartMs > SYNC_BUDGET_MS) {
    LOG_ERR("HWSYNC", "Sync budget exceeded");
    state = State::ERROR;
    errorMessage = tr(STR_HEADWATER_SYNC_FAILED);
    requestUpdate();
    return;
  }

  switch (state) {
    case State::CONNECTING: {
      if (checkSkip()) {
        finishToApp();
        return;
      }
      const wl_status_t status = WiFi.status();
      if (status == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
        state = State::FETCHING;
        requestUpdate();
      } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL ||
                 millis() - connectStartMs > CONNECT_TIMEOUT_MS) {
        state = State::ERROR;
        errorMessage = tr(STR_HEADWATER_SYNC_FAILED);
        requestUpdate();
      }
      break;
    }
    case State::FETCHING:
      if (checkSkip()) {
        finishToApp();
        return;
      }
      fetchAndQueue();  // blocking; small feed
      break;
    case State::DOWNLOADING:
      if (checkSkip()) {
        finishToApp();
        return;
      }
      if (currentIssue >= pending.size()) {
        state = State::DONE;
        statusMessage = tr(STR_HEADWATER_UP_TO_DATE);
        doneAtMs = millis();
        requestUpdate();
      } else {
        downloadNext();  // blocking; one issue per iteration
      }
      break;
    case State::ERROR:
      // Surface the error, then leave on any button.
      if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
          mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        finishToApp();
      }
      break;
    case State::DONE:
      // Hold the completion message briefly so the user sees it, but let an
      // impatient button press leave early.
      if (millis() - doneAtMs > DONE_HOLD_MS || mappedInput.wasReleased(MappedInputManager::Button::Back) ||
          mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        finishToApp();
      }
      break;
  }
}

void OpdsSyncActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const char* headerTitle = server.name.empty() ? tr(STR_HEADWATER_CHECK) : server.name.c_str();
  renderer.drawCenteredText(UI_12_FONT_ID, 15, headerTitle, true, EpdFontFamily::BOLD);

  if (state == State::DOWNLOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 40, tr(STR_DOWNLOADING));
    auto title = renderer.truncatedText(UI_10_FONT_ID, statusMessage.c_str(), pageWidth - 40);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 10, title.c_str());
    if (downloadTotal > 0) {
      GUI.drawProgressBar(renderer, Rect{50, pageHeight / 2 + 20, pageWidth - 100, 20}, downloadProgress, downloadTotal);
    }
  } else if (state == State::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, errorMessage.c_str());
  } else {
    // CONNECTING / FETCHING / DONE
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
