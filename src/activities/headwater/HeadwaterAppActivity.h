#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

// The Headwater "app": an offline browser over the issues the sync downloaded
// into /Headwater. The main page is an inbox: today's issue first (pre-selected
// so boot → Select starts reading), then Sync now and Channels. Older issues
// live behind "Archived"; user-sideloaded summaries behind "My Summaries". Both
// of those folders also feed the Channels view.
class HeadwaterAppActivity final : public Activity {
 public:
  explicit HeadwaterAppActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("HeadwaterApp", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // The rows the main page can show, in display order. Which subset is visible
  // depends on what's on disk (see buildRows()).
  enum class Row { Today, Sync, Channels, Archived, MySummaries };

  ButtonNavigator buttonNavigator;
  size_t selectorIndex = 0;
  // Issue file names (no path), newest first. Path is ISSUES_DIR + "/" + name.
  std::vector<std::string> issues;
  // True when /Headwater/My Summaries holds at least one EPUB.
  bool hasSaved = false;
  // True when a Headwater OPDS feed is configured (getHeadwaterServer()).
  bool hasFeed = false;
  // True when the user pressed Update without a feed configured: show the setup
  // help until they back out of it.
  bool viewingConnectHelp = false;
  // True when entered while Confirm was held (launched from the Home menu); we
  // swallow the next release so we don't immediately open the first issue.
  bool lockNextConfirmRelease = false;

  void reloadData();              // rescan issues + hasSaved + hasFeed (also auto-creates the folder)
  std::vector<Row> buildRows() const;
  // Fresh-flash / unconfigured state: no feed and nothing on disk yet, or the
  // user asked for setup help. Shows the "connect your account" screen instead
  // of the (empty) menu so the app is never a dead end after flashing.
  bool showConnectHelp() const { return viewingConnectHelp || (!hasFeed && issues.empty() && !hasSaved); }
  // Draws the battery chrome + masthead; returns the y just below the masthead.
  int drawMasthead() const;
  void renderConnectHelp();

  void onActivate(Row row);
  void onSelectIssue(const std::string& fileName);
  void onSelectSync();
  void openChannels();
  void openArchive();
  void openMySummaries();
  // Shared handler run when a sub-view (Channels/Archived/My Summaries) returns:
  // rescans disk so deletions there immediately update the main page row set.
  void onSubViewResult(const ActivityResult& result);
};
