#include "HeadwaterChannelIndex.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <map>
#include <set>

#include "HeadwaterManifest.h"
#include "HeadwaterPaths.h"

namespace headwater {

namespace {
// Scan one directory's top-level EPUBs (non-recursive), folding each manifest's
// items into byId. Missing directory or absent manifests are silently ignored so
// the two scan roots are independent. A video is one video: seenVideoIds keeps
// the first occurrence and drops repeats (e.g. a sent export that repeats a
// video already carried by a daily issue), so Channels never lists it twice.
void scanDir(const char* dirPath, std::map<std::string, Channel>& byId, std::set<std::string>& seenVideoIds) {
  auto dir = Storage.open(dirPath);
  if (!dir || !dir.isDirectory()) return;

  char nameBuf[256];
  dir.rewindDirectory();
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) continue;
    file.getName(nameBuf, sizeof(nameBuf));
    const std::string filename{nameBuf};
    if (!FsHelpers::hasEpubExtension(filename)) continue;

    const std::string path = std::string(dirPath) + "/" + filename;
    Manifest manifest;
    if (!loadManifest(path, manifest)) continue;

    for (const auto& item : manifest.items) {
      if (!item.videoId.empty() && !seenVideoIds.insert(item.videoId).second) continue;
      auto& ch = byId[item.channelId];
      if (ch.channelId.empty()) {
        ch.channelId   = item.channelId;
        ch.displayName = item.channel.empty() ? item.channelId : item.channel;
      }
      ch.entries.push_back({path, item.anchor, item.videoTitle, item.date, item.videoId});
    }
  }
}
}  // namespace

bool buildChannelIndex(ChannelIndex& out) {
  out.channels.clear();

  std::map<std::string, Channel> byId;
  std::set<std::string> seenVideoIds;  // dedup videos shared across issues + exports
  scanDir(ISSUES_DIR, byId, seenVideoIds);
  scanDir(MY_SUMMARIES_DIR, byId, seenVideoIds);

  if (byId.empty()) return false;

  out.channels.reserve(byId.size());
  for (auto& [id, ch] : byId) {
    std::sort(ch.entries.begin(), ch.entries.end(),
              [](const ChannelEntry& a, const ChannelEntry& b) { return a.date > b.date; });
    out.channels.push_back(std::move(ch));
  }
  std::sort(out.channels.begin(), out.channels.end(),
            [](const Channel& a, const Channel& b) { return a.displayName < b.displayName; });

  LOG_DBG("HWCIDX", "Built index: %zu channels", out.channels.size());
  return true;
}

}  // namespace headwater
