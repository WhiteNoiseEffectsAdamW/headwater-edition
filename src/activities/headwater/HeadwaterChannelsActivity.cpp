#include "HeadwaterChannelsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "HeadwaterChannelIndex.h"
#include "HeadwaterIdSet.h"
#include "HeadwaterNav.h"
#include "HeadwaterPaths.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/reader/ReaderUtils.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Max recent *read* daily summaries kept visible per channel. Unread entries and
// saved (My Summaries) entries are exempt, so this only trims read clutter.
constexpr size_t kRecentReadCap = 8;
}  // namespace

int HeadwaterChannelsActivity::channelRowCount() const {
  return static_cast<int>(index.channels.size()) + (mutedInfo.empty() ? 0 : 1);
}

bool HeadwaterChannelsActivity::onMutedRow() const {
  return !mutedInfo.empty() && channelIndex == index.channels.size();
}

int HeadwaterChannelsActivity::totalItems() const {
  if (mode == Mode::MENU) return static_cast<int>(menuActions.size());
  if (mode == Mode::MUTED) return static_cast<int>(mutedInfo.size());
  if (mode == Mode::CHANNELS) return channelRowCount();
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

  readIds = headwater::loadIdSet(headwater::READ_STATE);
  hiddenIds = headwater::loadIdSet(headwater::HIDDEN_STATE);
  mutedChannels = headwater::loadIdSet(headwater::MUTED_STATE);
  rebuild();

  loading = false;
  empty = index.channels.empty() && mutedInfo.empty();

  // Returning from a summary opened here: resume in that channel, cursor at the
  // next unread. Consume the hint either way so it can't linger.
  const std::string resumeId = headwater::channelsResumeChannelId();
  if (!resumeId.empty()) {
    headwater::clearChannelsResume();
    for (size_t i = 0; i < index.channels.size(); ++i) {
      if (index.channels[i].channelId == resumeId) {
        channelIndex = i;
        mode = Mode::ITEMS;
        itemIndex = firstUnread(index.channels[i]);
        break;
      }
    }
  }
  requestUpdate();
}

void HeadwaterChannelsActivity::onExit() {
  Activity::onExit();
  index.channels.clear();
}

void HeadwaterChannelsActivity::openSelected() {
  const auto& channel = index.channels[channelIndex];
  const auto& entry = channel.entries[itemIndex];
  // Mark-on-open: opening a summary counts as read (they're short).
  readIds.insert(entry.videoId);
  headwater::saveIdSet(headwater::READ_STATE, readIds);
  // Remember where we were so Back returns here instead of the app menu.
  headwater::setChannelsResume(channel.channelId);
  activityManager.goToReader(entry.path, entry.anchor);
}

void HeadwaterChannelsActivity::rebuild() {
  headwater::buildChannelIndex(index);
  applyCurationFilters();
  applyRecencyCap();
}

void HeadwaterChannelsActivity::applyCurationFilters() {
  // Capture muted channels (for the restore row/list), then drop them.
  mutedInfo.clear();
  for (const auto& ch : index.channels) {
    if (mutedChannels.count(ch.channelId) != 0) mutedInfo.push_back({ch.channelId, ch.displayName});
  }
  index.channels.erase(std::remove_if(index.channels.begin(), index.channels.end(),
                                      [this](const headwater::Channel& ch) {
                                        return mutedChannels.count(ch.channelId) != 0;
                                      }),
                       index.channels.end());
  // Drop hidden summaries, then drop any channel left empty.
  for (auto& ch : index.channels) {
    ch.entries.erase(std::remove_if(ch.entries.begin(), ch.entries.end(),
                                    [this](const headwater::ChannelEntry& e) {
                                      return hiddenIds.count(e.videoId) != 0;
                                    }),
                     ch.entries.end());
  }
  index.channels.erase(
      std::remove_if(index.channels.begin(), index.channels.end(),
                     [](const headwater::Channel& ch) { return ch.entries.empty(); }),
      index.channels.end());
}

void HeadwaterChannelsActivity::applyRecencyCap() {
  for (auto& ch : index.channels) {
    std::vector<headwater::ChannelEntry> kept;
    kept.reserve(ch.entries.size());
    size_t readKept = 0;
    for (auto& e : ch.entries) {  // newest-first
      const bool saved = e.path.rfind(headwater::MY_SUMMARIES_DIR, 0) == 0;
      if (isUnread(e) || saved) {
        kept.push_back(std::move(e));  // never hide unread or saved summaries
      } else if (readKept < kRecentReadCap) {
        kept.push_back(std::move(e));
        ++readKept;
      }
      // else: an older read daily summary — drop from Channels (still in its digest)
    }
    ch.entries = std::move(kept);
  }
}

bool HeadwaterChannelsActivity::channelHasUnread(const headwater::Channel& ch) const {
  for (const auto& e : ch.entries) {
    if (isUnread(e)) return true;
  }
  return false;
}

size_t HeadwaterChannelsActivity::firstUnread(const headwater::Channel& ch) const {
  for (size_t i = 0; i < ch.entries.size(); ++i) {
    if (isUnread(ch.entries[i])) return i;
  }
  return 0;
}

// ---- context menu -------------------------------------------------------

void HeadwaterChannelsActivity::openMenu() {
  menuSelector = 0;
  menuActions.clear();
  if (mode == Mode::ITEMS) {
    menuReturnMode = Mode::ITEMS;
    const auto& e = index.channels[channelIndex].entries[itemIndex];
    menuActions.push_back(isUnread(e) ? MenuAction::MarkRead : MenuAction::MarkUnread);
    menuActions.push_back(MenuAction::Hide);
  } else {
    menuReturnMode = Mode::CHANNELS;
    menuActions.push_back(MenuAction::MarkAllRead);
    menuActions.push_back(MenuAction::Mute);
  }
  menuActions.push_back(MenuAction::Cancel);
  mode = Mode::MENU;
  requestUpdate();
}

const char* HeadwaterChannelsActivity::menuLabel(MenuAction a) const {
  switch (a) {
    case MenuAction::MarkRead:     return tr(STR_HEADWATER_MARK_READ);
    case MenuAction::MarkUnread:   return tr(STR_HEADWATER_MARK_UNREAD);
    case MenuAction::Hide:         return tr(STR_HEADWATER_HIDE);
    case MenuAction::MarkAllRead:  return tr(STR_HEADWATER_MARK_ALL_READ);
    case MenuAction::Mute:         return tr(STR_HEADWATER_MUTE);
    case MenuAction::Cancel:       return tr(STR_HEADWATER_CANCEL);
  }
  return "";
}

void HeadwaterChannelsActivity::runMenuAction(MenuAction a) {
  switch (a) {
    case MenuAction::MarkRead:
    case MenuAction::MarkUnread:
      toggleItemRead(itemIndex);
      mode = Mode::ITEMS;
      break;
    case MenuAction::Hide:
      hideItem(itemIndex);  // sets mode itself (may empty the channel)
      break;
    case MenuAction::MarkAllRead:
      markChannelRead(channelIndex);
      mode = Mode::CHANNELS;
      break;
    case MenuAction::Mute:
      muteChannel(channelIndex);  // sets mode itself
      break;
    case MenuAction::Cancel:
      mode = menuReturnMode;
      break;
  }
  requestUpdate();
}

void HeadwaterChannelsActivity::toggleItemRead(size_t itemIdx) {
  const auto& e = index.channels[channelIndex].entries[itemIdx];
  if (isUnread(e)) {
    readIds.insert(e.videoId);
  } else {
    readIds.erase(e.videoId);
  }
  headwater::saveIdSet(headwater::READ_STATE, readIds);
}

void HeadwaterChannelsActivity::markChannelRead(size_t channelIdx) {
  for (const auto& e : index.channels[channelIdx].entries) readIds.insert(e.videoId);
  headwater::saveIdSet(headwater::READ_STATE, readIds);
}

void HeadwaterChannelsActivity::hideItem(size_t itemIdx) {
  auto& ch = index.channels[channelIndex];
  hiddenIds.insert(ch.entries[itemIdx].videoId);
  headwater::saveIdSet(headwater::HIDDEN_STATE, hiddenIds);
  ch.entries.erase(ch.entries.begin() + static_cast<long>(itemIdx));

  if (ch.entries.empty()) {
    // Channel emptied — drop it and fall back to the channel list.
    index.channels.erase(index.channels.begin() + static_cast<long>(channelIndex));
    if (channelIndex >= index.channels.size()) channelIndex = index.channels.empty() ? 0 : index.channels.size() - 1;
    itemIndex = 0;
    mode = Mode::CHANNELS;
  } else {
    if (itemIndex >= ch.entries.size()) itemIndex = ch.entries.size() - 1;
    mode = Mode::ITEMS;
  }
  empty = index.channels.empty() && mutedInfo.empty();
}

void HeadwaterChannelsActivity::muteChannel(size_t channelIdx) {
  auto& ch = index.channels[channelIdx];
  mutedChannels.insert(ch.channelId);
  headwater::saveIdSet(headwater::MUTED_STATE, mutedChannels);
  mutedInfo.push_back({ch.channelId, ch.displayName});  // surface it in the restore row now
  index.channels.erase(index.channels.begin() + static_cast<long>(channelIdx));
  if (channelIndex >= index.channels.size()) channelIndex = index.channels.empty() ? 0 : index.channels.size() - 1;
  itemIndex = 0;
  mode = Mode::CHANNELS;
  empty = index.channels.empty() && mutedInfo.empty();
}

void HeadwaterChannelsActivity::unmuteSelected() {
  if (itemIndex >= mutedInfo.size()) return;
  mutedChannels.erase(mutedInfo[itemIndex].channelId);
  headwater::saveIdSet(headwater::MUTED_STATE, mutedChannels);
  rebuild();  // re-scan so the channel returns to the list; mutedInfo regenerated
  mode = Mode::CHANNELS;
  channelIndex = 0;
  itemIndex = 0;
  empty = index.channels.empty() && mutedInfo.empty();
  requestUpdate();
}

// ---- input --------------------------------------------------------------

void HeadwaterChannelsActivity::loop() {
  if (loading) return;

  const int total = empty ? 0 : totalItems();
  const int pageItems = UITheme::getNumberOfItemsPerPage(renderer, true, false, true, false);

  // Context menu: Select runs the highlighted action, Back cancels.
  if (mode == Mode::MENU) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (menuSelector < menuActions.size()) runMenuAction(menuActions[menuSelector]);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      mode = menuReturnMode;
      requestUpdate();
      return;
    }
    buttonNavigator.onNextRelease([this, total] {
      menuSelector = static_cast<size_t>(ButtonNavigator::nextIndex(static_cast<int>(menuSelector), total));
      requestUpdate();
    });
    buttonNavigator.onPreviousRelease([this, total] {
      menuSelector = static_cast<size_t>(ButtonNavigator::previousIndex(static_cast<int>(menuSelector), total));
      requestUpdate();
    });
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!empty) {
      // Long-press opens the curation menu; a tap opens / drills in.
      const bool longPress = mappedInput.getHeldTime() >= ReaderUtils::GO_HOME_MS;
      if (mode == Mode::MUTED) {
        unmuteSelected();  // tap restores the highlighted channel
      } else if (mode == Mode::CHANNELS) {
        if (onMutedRow()) {
          if (!longPress) {
            mode = Mode::MUTED;
            itemIndex = 0;
            requestUpdate();
          }
        } else if (longPress) {
          openMenu();
        } else {
          itemIndex = 0;
          mode = Mode::ITEMS;
          requestUpdate();
        }
      } else {  // ITEMS
        if (longPress) {
          openMenu();
        } else {
          openSelected();
        }
      }
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (mode == Mode::ITEMS || mode == Mode::MUTED) {
      mode = Mode::CHANNELS;
      itemIndex = 0;
      requestUpdate();
    } else {  // CHANNELS
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

// ---- render -------------------------------------------------------------

void HeadwaterChannelsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  const char* header = tr(STR_HEADWATER_CHANNELS);
  if (!empty && mode == Mode::ITEMS) {
    header = index.channels[channelIndex].displayName.c_str();
  } else if (mode == Mode::MUTED) {
    header = tr(STR_HEADWATER_MUTED_CHANNELS);
  } else if (mode == Mode::MENU) {
    header = (menuReturnMode == Mode::ITEMS) ? index.channels[channelIndex].entries[itemIndex].videoTitle.c_str()
                                             : index.channels[channelIndex].displayName.c_str();
  }
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, header);

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const Rect content{0, contentTop, pageWidth, contentHeight};

  if (loading) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_LOADING));
  } else if (empty) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_HEADWATER_NO_ISSUES));
  } else if (mode == Mode::MENU) {
    GUI.drawList(renderer, content, static_cast<int>(menuActions.size()), static_cast<int>(menuSelector),
                 [this](int i) -> std::string { return menuLabel(menuActions[i]); });
  } else if (mode == Mode::MUTED) {
    GUI.drawList(renderer, content, static_cast<int>(mutedInfo.size()), static_cast<int>(itemIndex),
                 [this](int i) -> std::string { return mutedInfo[i].name; });
  } else if (mode == Mode::CHANNELS) {
    // rowValue is the only per-row hook every theme renders, so the unread mark
    // rides there (right-aligned dot; the theme handles selection inversion). The
    // trailing "Muted channels (N)" row (when any are muted) carries no dot.
    const int channelCount = static_cast<int>(index.channels.size());
    GUI.drawList(renderer, content, channelRowCount(), static_cast<int>(channelIndex),
                 [this, channelCount](int i) -> std::string {
                   if (i < channelCount) return index.channels[i].displayName;
                   return std::string(tr(STR_HEADWATER_MUTED_CHANNELS)) + " (" + std::to_string(mutedInfo.size()) + ")";
                 },
                 nullptr, nullptr,
                 [this, channelCount](int i) -> std::string {
                   if (i < channelCount && channelHasUnread(index.channels[i])) return tr(STR_HEADWATER_UNREAD_DOT);
                   return std::string();
                 });
  } else {
    const auto& entries = index.channels[channelIndex].entries;
    GUI.drawList(renderer, content, static_cast<int>(entries.size()), static_cast<int>(itemIndex),
                 [&entries](int i) -> std::string { return entries[i].videoTitle; }, nullptr, nullptr,
                 [this, &entries](int i) -> std::string {
                   return isUnread(entries[i]) ? tr(STR_HEADWATER_UNREAD_DOT) : std::string();
                 });
  }

  const int cnt = empty ? 0 : totalItems();
  // In the muted list the Select button restores a channel — say so, so it's not
  // mistaken for "open".
  const char* selectLabel = (mode == Mode::MUTED) ? tr(STR_HEADWATER_UNMUTE) : tr(STR_SELECT);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), (!empty && !loading) ? selectLabel : "",
                                            cnt > 1 ? tr(STR_DIR_UP) : "", cnt > 1 ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
