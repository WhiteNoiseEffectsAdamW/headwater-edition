#include "HeadwaterChannelsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "HeadwaterChannelIndex.h"
#include "HeadwaterPaths.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

int HeadwaterChannelsActivity::totalItems() const {
  if (mode == Mode::CHANNELS) return static_cast<int>(index.channels.size());
  return static_cast<int>(index.channels[channelIndex].entries.size());
}

void HeadwaterChannelsActivity::onEnter() {
  Activity::onEnter();
  loading = true;
  empty = false;
  mode = Mode::CHANNELS;
  channelIndex = 0;
  itemIndex = 0;
  requestUpdate();  // paint "Loading…" before blocking

  headwater::buildChannelIndex(index);

  loading = false;
  empty = index.channels.empty();
  requestUpdate();
}

void HeadwaterChannelsActivity::onExit() {
  Activity::onExit();
  index.channels.clear();
}

void HeadwaterChannelsActivity::openSelected() {
  const auto& entry = index.channels[channelIndex].entries[itemIndex];
  activityManager.goToReader(entry.path, entry.anchor);
}

void HeadwaterChannelsActivity::loop() {
  if (loading) return;

  const int total = empty ? 0 : totalItems();
  const int pageItems = UITheme::getNumberOfItemsPerPage(renderer, true, false, true, false);

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!empty) {
      if (mode == Mode::CHANNELS) {
        channelIndex =
            static_cast<size_t>(std::min(static_cast<int>(channelIndex), static_cast<int>(index.channels.size()) - 1));
        itemIndex = 0;
        mode = Mode::ITEMS;
        requestUpdate();
      } else {
        openSelected();
      }
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (mode == Mode::ITEMS) {
      mode = Mode::CHANNELS;
      itemIndex = 0;
      requestUpdate();
    } else {
      finish();
    }
    return;
  }

  if (total == 0) return;

  if (mode == Mode::CHANNELS) {
    buttonNavigator.onNextRelease([this, total] {
      channelIndex = static_cast<size_t>(ButtonNavigator::nextIndex(static_cast<int>(channelIndex), total));
      requestUpdate();
    });
    buttonNavigator.onPreviousRelease([this, total] {
      channelIndex = static_cast<size_t>(ButtonNavigator::previousIndex(static_cast<int>(channelIndex), total));
      requestUpdate();
    });
    buttonNavigator.onNextContinuous([this, total, pageItems] {
      channelIndex = static_cast<size_t>(ButtonNavigator::nextPageIndex(static_cast<int>(channelIndex), total, pageItems));
      requestUpdate();
    });
    buttonNavigator.onPreviousContinuous([this, total, pageItems] {
      channelIndex = static_cast<size_t>(ButtonNavigator::previousPageIndex(static_cast<int>(channelIndex), total, pageItems));
      requestUpdate();
    });
  } else {
    buttonNavigator.onNextRelease([this, total] {
      itemIndex = static_cast<size_t>(ButtonNavigator::nextIndex(static_cast<int>(itemIndex), total));
      requestUpdate();
    });
    buttonNavigator.onPreviousRelease([this, total] {
      itemIndex = static_cast<size_t>(ButtonNavigator::previousIndex(static_cast<int>(itemIndex), total));
      requestUpdate();
    });
    buttonNavigator.onNextContinuous([this, total, pageItems] {
      itemIndex = static_cast<size_t>(ButtonNavigator::nextPageIndex(static_cast<int>(itemIndex), total, pageItems));
      requestUpdate();
    });
    buttonNavigator.onPreviousContinuous([this, total, pageItems] {
      itemIndex = static_cast<size_t>(ButtonNavigator::previousPageIndex(static_cast<int>(itemIndex), total, pageItems));
      requestUpdate();
    });
  }
}

void HeadwaterChannelsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  const char* header = (!empty && mode == Mode::ITEMS)
                           ? index.channels[channelIndex].displayName.c_str()
                           : tr(STR_HEADWATER_CHANNELS);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header);

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (loading) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_LOADING));
  } else if (empty) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_HEADWATER_NO_ISSUES));
  } else if (mode == Mode::CHANNELS) {
    const int selIdx = static_cast<int>(channelIndex);
    const int total = static_cast<int>(index.channels.size());
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, total, selIdx,
                 [this](int i) -> std::string { return index.channels[i].displayName; });
  } else {
    const auto& entries = index.channels[channelIndex].entries;
    const int selIdx = static_cast<int>(itemIndex);
    GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight},
                 static_cast<int>(entries.size()), selIdx,
                 [&entries](int i) -> std::string { return entries[i].videoTitle; });
  }

  const int cnt = empty ? 0 : totalItems();
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), (!empty && !loading) ? tr(STR_SELECT) : "",
                            cnt > 1 ? tr(STR_DIR_UP) : "", cnt > 1 ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
