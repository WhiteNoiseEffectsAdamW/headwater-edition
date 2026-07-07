#include "HeadwaterAppActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <string_view>

#include "HeadwaterChannelsActivity.h"
#include "HeadwaterFolderActivity.h"
#include "HeadwaterNav.h"
#include "HeadwaterPaths.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "activities/ActivityManager.h"
#include "activities/network/OpdsSyncActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/HeadwaterHeader.h"

namespace {
constexpr size_t NAME_BUFFER_SIZE = 256;

std::string displayName(const std::string& fileName) {
  const auto pos = fileName.rfind('.');
  return pos == std::string::npos ? fileName : fileName.substr(0, pos);
}

// The masthead already says "Headwater", so drop that from the issue's own name
// for the Today row: "Headwater Daily 2026-06-29" -> "Daily 2026-06-29".
std::string issueLabel(const std::string& fileName) {
  std::string name = displayName(fileName);
  for (const char* prefix : {"Headwater - ", "Headwater "}) {
    const std::string_view p{prefix};
    if (name.rfind(p, 0) == 0) name.erase(0, p.size());
  }
  return name;
}

// Top-level EPUB file names in `dir`, newest-first (descending name sort).
// Subdirectories are skipped, so My Summaries never leaks into the issue list.
void scanEpubNames(const char* dir, std::vector<std::string>& out) {
  out.clear();
  auto d = Storage.open(dir);
  if (!d || !d.isDirectory()) return;

  char nameBuf[NAME_BUFFER_SIZE];
  d.rewindDirectory();
  for (auto file = d.openNextFile(); file; file = d.openNextFile()) {
    if (file.isDirectory()) continue;
    file.getName(nameBuf, NAME_BUFFER_SIZE);
    const std::string_view name{nameBuf};
    if (FsHelpers::hasEpubExtension(name)) out.emplace_back(name);
  }
  std::sort(out.begin(), out.end(), std::greater<std::string>());
}

bool folderHasEpub(const char* dir) {
  auto d = Storage.open(dir);
  if (!d || !d.isDirectory()) return false;

  char nameBuf[NAME_BUFFER_SIZE];
  d.rewindDirectory();
  for (auto file = d.openNextFile(); file; file = d.openNextFile()) {
    if (file.isDirectory()) continue;
    file.getName(nameBuf, NAME_BUFFER_SIZE);
    if (FsHelpers::hasEpubExtension(std::string_view{nameBuf})) return true;
  }
  return false;
}
}  // namespace

void HeadwaterAppActivity::reloadData() {
  // Make the sideload destination exist so it shows up over USB/Wi-Fi transfer.
  Storage.ensureDirectoryExists(headwater::MY_SUMMARIES_DIR);
  scanEpubNames(headwater::ISSUES_DIR, issues);
  hasSaved = folderHasEpub(headwater::MY_SUMMARIES_DIR);
  hasFeed = OPDS_STORE.getHeadwaterServer() != nullptr;
}

std::vector<HeadwaterAppActivity::Row> HeadwaterAppActivity::buildRows() const {
  std::vector<Row> rows;
  const bool hasIssues = !issues.empty();
  if (hasIssues) rows.push_back(Row::Today);
  rows.push_back(Row::Sync);
  if (hasIssues || hasSaved) rows.push_back(Row::Channels);
  if (hasSaved) rows.push_back(Row::MySummaries);
  if (issues.size() > 1) rows.push_back(Row::Archived);
  return rows;
}

void HeadwaterAppActivity::onEnter() {
  Activity::onEnter();
  // Pre-select today's issue when one exists; otherwise the first available row.
  selectorIndex = 0;
  viewingConnectHelp = false;
  // Launched from the Home menu with Confirm held: swallow that release so we
  // don't immediately open today's issue.
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  reloadData();
  // Returning from a summary opened in Channels: re-open Channels where we left
  // off instead of showing the menu. (Cleared on reaching Home, so this only
  // fires on the direct reader -> app hop.)
  if (!headwater::channelsResumeChannelId().empty()) {
    openChannels();
    return;
  }
  requestUpdate();
}

void HeadwaterAppActivity::onExit() {
  Activity::onExit();
  issues.clear();
}

void HeadwaterAppActivity::onSelectSync() {
  const OpdsServer* server = OPDS_STORE.getHeadwaterServer();
  if (server) {
    activityManager.replaceActivity(std::make_unique<OpdsSyncActivity>(renderer, mappedInput, *server));
    return;
  }
  // No feed configured yet: guide the user through setup instead of no-op'ing.
  viewingConnectHelp = true;
  requestUpdate(true);
}

void HeadwaterAppActivity::onSelectIssue(const std::string& fileName) {
  activityManager.goToReader(std::string(headwater::ISSUES_DIR) + "/" + fileName);
}

void HeadwaterAppActivity::openChannels() {
  startActivityForResult(std::make_unique<HeadwaterChannelsActivity>(renderer, mappedInput),
                         [this](const ActivityResult& r) { onSubViewResult(r); });
}

void HeadwaterAppActivity::openArchive() {
  startActivityForResult(
      std::make_unique<HeadwaterFolderActivity>(renderer, mappedInput, headwater::ISSUES_DIR,
                                                tr(STR_HEADWATER_ARCHIVE), /*reserveNewest=*/true,
                                                tr(STR_HEADWATER_DELETE_WARNING), tr(STR_HEADWATER_NO_ISSUES)),
      [this](const ActivityResult& r) { onSubViewResult(r); });
}

void HeadwaterAppActivity::openMySummaries() {
  startActivityForResult(
      std::make_unique<HeadwaterFolderActivity>(renderer, mappedInput, headwater::MY_SUMMARIES_DIR,
                                                tr(STR_HEADWATER_MY_SUMMARIES), /*reserveNewest=*/false,
                                                tr(STR_HEADWATER_DELETE_WARNING), tr(STR_HEADWATER_NO_SAVED)),
      [this](const ActivityResult& r) { onSubViewResult(r); });
}

void HeadwaterAppActivity::onSubViewResult(const ActivityResult&) {
  reloadData();
  const int total = static_cast<int>(buildRows().size());
  if (total == 0) {
    selectorIndex = 0;
  } else if (selectorIndex >= static_cast<size_t>(total)) {
    selectorIndex = static_cast<size_t>(total) - 1;
  }
  requestUpdate(true);
}

void HeadwaterAppActivity::onActivate(Row row) {
  switch (row) {
    case Row::Today:       onSelectIssue(issues[0]); break;
    case Row::Sync:        onSelectSync();           break;
    case Row::Channels:    openChannels();           break;
    case Row::Archived:    openArchive();            break;
    case Row::MySummaries: openMySummaries();        break;
  }
}

void HeadwaterAppActivity::loop() {
  // Setup-help screen: Back dismisses the help (if the user opened it over a
  // populated menu) or leaves to Home (fresh-flash state). No menu nav.
  if (showConnectHelp()) {
    // Swallow the Confirm release that launched us from the Home menu.
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && lockNextConfirmRelease) {
      lockNextConfirmRelease = false;
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      if (viewingConnectHelp) {
        viewingConnectHelp = false;
        requestUpdate(true);
      } else {
        onGoHome();
      }
    }
    return;
  }

  const auto rows = buildRows();
  const int totalItems = static_cast<int>(rows.size());
  const int pageItems = UITheme::getNumberOfItemsPerPage(renderer, true, false, true, false);

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (lockNextConfirmRelease) {
      lockNextConfirmRelease = false;
      return;
    }
    if (selectorIndex < rows.size()) onActivate(rows[selectorIndex]);
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  buttonNavigator.onNextRelease([this, totalItems] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), totalItems);
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, totalItems] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), totalItems);
    requestUpdate();
  });
  buttonNavigator.onNextContinuous([this, totalItems, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), totalItems, pageItems);
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this, totalItems, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), totalItems, pageItems);
    requestUpdate();
  });
}

int HeadwaterAppActivity::drawMasthead() const {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto& metrics  = UITheme::getInstance().getMetrics();

  // Battery chrome only (nullptr title); the masthead graphic below is our brand.
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, nullptr);

  // "Headwater" masthead (baked 1-bit artwork). Stored pre-rotated 90deg, so on
  // screen it renders HeadwaterHeaderHeight wide x HeadwaterHeaderWidth tall. For
  // a rotated drawImage the on-screen y becomes the framebuffer byte-column
  // origin, so it must be a multiple of 8 (hence the & ~7).
  const int mastheadW = HeadwaterHeaderHeight;  // on-screen width
  const int mastheadH = HeadwaterHeaderWidth;   // on-screen height
  const int mastheadX = (pageWidth - mastheadW) / 2;
  // Hug the battery: the header band reserves 45px but the battery glyph is only
  // ~12px tall (drawn at y+5), leaving dead space. Sit just under the glyph.
  const int batteryBottom = metrics.topPadding + 5 + metrics.batteryHeight;
  const int mastheadY     = ((batteryBottom + 7) & ~7) + 8;  // one byte-row of clearance
  renderer.drawImage(HeadwaterHeader, mastheadX, mastheadY, HeadwaterHeaderWidth, HeadwaterHeaderHeight);
  return mastheadY + mastheadH;
}

void HeadwaterAppActivity::renderConnectHelp() {
  const int belowMasthead = drawMasthead();
  const auto& metrics = UITheme::getInstance().getMetrics();

  int y = belowMasthead + metrics.homeMenuTopOffset + 24;
  renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_HEADWATER_SETUP_TITLE), true, EpdFontFamily::BOLD);
  y += 44;
  for (const char* line : {tr(STR_HEADWATER_SETUP_L1), tr(STR_HEADWATER_SETUP_L2), tr(STR_HEADWATER_SETUP_L3),
                           tr(STR_HEADWATER_SETUP_L4), tr(STR_HEADWATER_SETUP_L5)}) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, line);
    y += 28;
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void HeadwaterAppActivity::render(RenderLock&&) {
  // Fresh flash / no feed configured: show setup guidance, never a dead menu.
  if (showConnectHelp()) {
    renderConnectHelp();
    return;
  }

  const int belowMasthead = drawMasthead();
  const auto pageWidth  = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics   = UITheme::getInstance().getMetrics();

  // Only 4-ish rows, so give the menu a little air below the masthead.
  const int menuTop = belowMasthead + metrics.homeMenuTopOffset + 18;
  const int menuHeight = pageHeight - menuTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const auto rows      = buildRows();
  const int totalItems = static_cast<int>(rows.size());

  GUI.drawButtonMenu(
      renderer, Rect{0, menuTop, pageWidth, menuHeight}, totalItems, static_cast<int>(selectorIndex),
      [this, &rows](int i) -> std::string {
        switch (rows[i]) {
          case Row::Today:       return issueLabel(issues[0]);
          case Row::Sync:        return tr(STR_HEADWATER_SYNC_NOW);
          case Row::Channels:    return tr(STR_HEADWATER_CHANNELS);
          case Row::Archived:    return tr(STR_HEADWATER_ARCHIVE);
          case Row::MySummaries: return tr(STR_HEADWATER_MY_SUMMARIES);
        }
        return {};
      },
      [&rows](int i) -> UIIcon {
        switch (rows[i]) {
          case Row::Today:       return Book;      // the issue you open and read
          case Row::Sync:        return Wifi;      // the network pull
          case Row::Channels:    return Library;   // browse the collection
          case Row::Archived:    return Folder;    // older-issues drawer
          case Row::MySummaries: return Recent;    // booklet+ribbon: saved / curated
        }
        return None;
      });

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT),
                                            totalItems > 1 ? tr(STR_DIR_UP) : "",
                                            totalItems > 1 ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
