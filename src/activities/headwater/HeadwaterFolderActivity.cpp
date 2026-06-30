#include "HeadwaterFolderActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <string_view>

#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/reader/ReaderUtils.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/BookCacheUtils.h"

namespace {
constexpr size_t NAME_BUFFER_SIZE = 256;

std::string displayName(const std::string& fileName) {
  const auto pos = fileName.rfind('.');
  return pos == std::string::npos ? fileName : fileName.substr(0, pos);
}
}  // namespace

void HeadwaterFolderActivity::loadEntries() {
  entries.clear();
  auto dir = Storage.open(folderPath.c_str());
  if (!dir || !dir.isDirectory()) return;

  char nameBuf[NAME_BUFFER_SIZE];
  dir.rewindDirectory();
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) continue;
    file.getName(nameBuf, NAME_BUFFER_SIZE);
    const std::string_view name{nameBuf};
    if (FsHelpers::hasEpubExtension(name)) entries.emplace_back(name);
  }
  std::sort(entries.begin(), entries.end(), std::greater<std::string>());

  // "Archived" hides the newest issue (shown on the app page as "Today").
  if (reserveNewest && !entries.empty()) entries.erase(entries.begin());
}

void HeadwaterFolderActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  lockNextConfirmRelease = mappedInput.isPressed(MappedInputManager::Button::Confirm);
  loadEntries();
  requestUpdate();
}

void HeadwaterFolderActivity::onExit() {
  Activity::onExit();
  entries.clear();
}

void HeadwaterFolderActivity::loop() {
  const int total = count();
  const int pageItems = UITheme::getNumberOfItemsPerPage(renderer, true, false, true, false);

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (lockNextConfirmRelease) {
      lockNextConfirmRelease = false;
      return;
    }
    if (total == 0) return;

    const std::string& fileName = entries[selectorIndex];
    const std::string fullPath = folderPath + "/" + fileName;

    if (mappedInput.getHeldTime() >= ReaderUtils::GO_HOME_MS) {
      // Long hold: delete with Channels warning.
      const std::string heading = std::string(tr(STR_DELETE)) + "? " + displayName(fileName);
      startActivityForResult(
          std::make_unique<ConfirmationActivity>(renderer, mappedInput, heading, deleteWarning),
          [this, fullPath](const ActivityResult& res) {
            if (!res.isCancelled) {
              clearBookCache(fullPath);
              Storage.remove(fullPath.c_str());
              loadEntries();
              if (count() == 0) {
                finish();  // folder empty — go back to app
              } else if (selectorIndex >= static_cast<size_t>(count())) {
                selectorIndex = static_cast<size_t>(count()) - 1;
              }
              requestUpdate(true);
            }
          });
    } else {
      activityManager.goToReader(fullPath);
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (total == 0) return;

  buttonNavigator.onNextRelease([this, total] {
    selectorIndex = static_cast<size_t>(ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), total));
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this, total] {
    selectorIndex = static_cast<size_t>(ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), total));
    requestUpdate();
  });
  buttonNavigator.onNextContinuous([this, total, pageItems] {
    selectorIndex = static_cast<size_t>(ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), total, pageItems));
    requestUpdate();
  });
  buttonNavigator.onPreviousContinuous([this, total, pageItems] {
    selectorIndex = static_cast<size_t>(ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), total, pageItems));
    requestUpdate();
  });
}

void HeadwaterFolderActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header.c_str());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const int total = count();
  if (total == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, emptyLabel.c_str());
  } else {
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, total, static_cast<int>(selectorIndex),
                 [this](int i) -> std::string { return displayName(entries[static_cast<size_t>(i)]); });
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), total > 0 ? tr(STR_SELECT) : "",
                                            total > 1 ? tr(STR_DIR_UP) : "", total > 1 ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
