#include "HeadwaterAppActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <string_view>

#include "HeadwaterChannelsActivity.h"
#include "HeadwaterFolderActivity.h"
#include "HeadwaterPaths.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "activities/ActivityManager.h"
#include "activities/network/OpdsSyncActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr size_t NAME_BUFFER_SIZE = 256;

std::string displayName(const std::string& fileName) {
  const auto pos = fileName.rfind('.');
  return pos == std::string::npos ? fileName : fileName.substr(0, pos);
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
}

std::vector<HeadwaterAppActivity::Row> HeadwaterAppActivity::buildRows() const {
  std::vector<Row> rows;
  const bool hasIssues = !issues.empty();
  if (hasIssues) rows.push_back(Row::Today);
  rows.push_back(Row::Sync);
  if (hasIssues || hasSaved) rows.push_back(Row::Channels);
  if (issues.size() > 1) rows.push_back(Row::Archived);
  if (hasSaved) rows.push_back(Row::MySummaries);
  return rows;
}

void HeadwaterAppActivity::onEnter() {
  Activity::onEnter();
  // Pre-select today's issue when one exists; otherwise the first available row.
  selectorIndex = 0;
  // Launched from the Home menu with Confirm held: swallow that release so we
  // don't immediately open today's issue.
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  reloadData();
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
  }
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

void HeadwaterAppActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth  = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics   = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_HEADWATER));

  const int contentTop    = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const auto rows      = buildRows();
  const int totalItems = static_cast<int>(rows.size());

  GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, totalItems,
               static_cast<int>(selectorIndex), [this, &rows](int i) -> std::string {
                 switch (rows[i]) {
                   case Row::Today:       return displayName(issues[0]);
                   case Row::Sync:        return tr(STR_HEADWATER_SYNC_NOW);
                   case Row::Channels:    return tr(STR_HEADWATER_CHANNELS);
                   case Row::Archived:    return tr(STR_HEADWATER_ARCHIVE);
                   case Row::MySummaries: return tr(STR_HEADWATER_MY_SUMMARIES);
                 }
                 return {};
               });

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT),
                                            totalItems > 1 ? tr(STR_DIR_UP) : "",
                                            totalItems > 1 ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
