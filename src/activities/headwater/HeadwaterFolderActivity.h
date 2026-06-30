#pragma once
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// A flat, newest-first browser over the EPUBs in one Headwater folder.
//   Short Select   → open in reader.
//   Long-hold Select → delete (with an optional Channels-removal warning).
// Launched via pushActivity so Back returns to the parent (HeadwaterAppActivity).
//
// Two call sites share this one class:
//   - "Archived"     folder=/Headwater,             reserveNewest=true  (hides
//                    the newest issue, which lives on the app page as "Today").
//   - "My Summaries" folder=/Headwater/My Summaries, reserveNewest=false (all).
//
// Both folders now feed Channels, so both pass the same delete warning.
class HeadwaterFolderActivity final : public Activity {
 public:
  HeadwaterFolderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string folderPath,
                          std::string header, bool reserveNewest, std::string deleteWarning, std::string emptyLabel)
      : Activity("HeadwaterFolder", renderer, mappedInput),
        folderPath(std::move(folderPath)),
        header(std::move(header)),
        reserveNewest(reserveNewest),
        deleteWarning(std::move(deleteWarning)),
        emptyLabel(std::move(emptyLabel)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  const std::string folderPath;
  const std::string header;
  const bool reserveNewest;
  const std::string deleteWarning;
  const std::string emptyLabel;

  // Visible EPUB file names (no path), newest-first. When reserveNewest is set
  // the newest file is dropped here so the rest of the logic needs no offset.
  std::vector<std::string> entries;
  size_t selectorIndex = 0;
  bool lockNextConfirmRelease = false;
  ButtonNavigator buttonNavigator;

  void loadEntries();
  int count() const { return static_cast<int>(entries.size()); }
};
