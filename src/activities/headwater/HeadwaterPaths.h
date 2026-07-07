#pragma once

// Single source of truth for where Headwater issues live on the SD card.
// OpdsSyncActivity writes issues here; HeadwaterAppActivity reads them back.
// Kept separate from the SD root so issues don't litter the file browser and so
// the Headwater app can scope to one place.
namespace headwater {
inline constexpr char ISSUES_DIR[] = "/Headwater";
// User-sideloaded summary EPUBs live here. Kept as a subfolder of ISSUES_DIR so
// loadIssues() (which skips directories) never surfaces them in the daily inbox,
// while buildChannelIndex() scans it explicitly so saved summaries still merge
// into Channels alongside the daily digests.
inline constexpr char MY_SUMMARIES_DIR[] = "/Headwater/My Summaries";

// Hidden sidecar files for Channels curation state (not .epub, so the issue and
// channel scans skip them). See HeadwaterIdSet.
inline constexpr char READ_STATE[] = "/Headwater/.read";      // read videoIds
inline constexpr char HIDDEN_STATE[] = "/Headwater/.hidden";  // hidden videoIds
inline constexpr char MUTED_STATE[] = "/Headwater/.muted";    // muted channelIds
}  // namespace headwater
