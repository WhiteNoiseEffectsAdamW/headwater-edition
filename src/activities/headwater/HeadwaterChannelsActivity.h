#pragma once
#include <set>
#include <string>
#include <vector>

#include "HeadwaterChannelIndex.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// Two-level browser: channels list → per-channel summary entries.
// Selecting a summary deep-links into the owning digest EPUB at that anchor.
// Launched via pushActivity so Back returns to HeadwaterAppActivity.
class HeadwaterChannelsActivity final : public Activity {
 public:
  explicit HeadwaterChannelsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("HeadwaterChannels", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Mode { CHANNELS, ITEMS, MENU, MUTED };
  // Long-press context menu actions; the visible set depends on where it opened.
  enum class MenuAction { MarkRead, MarkUnread, Hide, MarkAllRead, Mute, Cancel };
  struct MutedRef {
    std::string channelId;
    std::string name;
  };

  headwater::ChannelIndex index;
  Mode mode = Mode::CHANNELS;
  size_t channelIndex = 0;
  size_t itemIndex = 0;
  bool loading = true;
  bool empty = false;

  // Curation state, loaded on enter and written back on change.
  std::set<std::string> readIds;         // read videoIds
  std::set<std::string> hiddenIds;       // hidden videoIds (removed from Channels)
  std::set<std::string> mutedChannels;   // muted channelIds (removed from Channels)
  // Display refs for muted channels, so the "Muted channels (N)" restore row and
  // the un-mute list have names to show. Rebuilt alongside the index.
  std::vector<MutedRef> mutedInfo;

  // Context-menu state (Mode::MENU).
  std::vector<MenuAction> menuActions;
  size_t menuSelector = 0;
  Mode menuReturnMode = Mode::ITEMS;  // where Cancel/most actions go back to

  ButtonNavigator buttonNavigator;

  void openSelected();
  int totalItems() const;
  // Channel-list rows = real channels + a trailing "Muted channels" row when any
  // are muted. The muted row's index is index.channels.size().
  int channelRowCount() const;
  bool onMutedRow() const;
  void rebuild();          // (re)scan the index and re-apply curation filters
  void unmuteSelected();   // un-mute the highlighted row in the muted list
  bool channelHasUnread(const headwater::Channel& ch) const;
  bool isUnread(const headwater::ChannelEntry& e) const { return readIds.count(e.videoId) == 0; }
  size_t firstUnread(const headwater::Channel& ch) const;  // index of first unread entry, else 0

  // Curation actions (menu-driven).
  void openMenu();                 // long-press: build + show the context menu
  void runMenuAction(MenuAction a);
  const char* menuLabel(MenuAction a) const;
  void toggleItemRead(size_t itemIdx);
  void markChannelRead(size_t channelIdx);
  void hideItem(size_t itemIdx);       // drop from Channels (kept in its digest)
  void muteChannel(size_t channelIdx);

  // Filter the freshly-built index by the curation state, then trim read clutter.
  void applyCurationFilters();  // remove muted channels + hidden entries + empties
  void applyRecencyCap();       // cap recent read per channel (unread/saved exempt)
};
