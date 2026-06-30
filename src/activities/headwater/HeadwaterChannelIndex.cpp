#include "HeadwaterChannelIndex.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <map>

#include "HeadwaterManifest.h"
#include "HeadwaterPaths.h"

namespace headwater {

namespace {
// Scan one directory's top-level EPUBs (non-recursive), folding each manifest's
// items into byId. Missing directory or absent manifests are silently ignored so
// the two scan roots are independent.
void scanDir(const char* dirPath, std::map<std::string, Channel>& byId) {
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
      auto& ch = byId[item.channelId];
      if (ch.channelId.empty()) {
        ch.channelId   = item.channelId;
        ch.displayName = item.channel.empty() ? item.channelId : item.channel;
      }
      ch.entries.push_back({path, item.anchor, item.videoTitle, item.date});
    }
  }
}
}  // namespace

bool buildChannelIndex(ChannelIndex& out) {
  out.channels.clear();

  std::map<std::string, Channel> byId;
  scanDir(ISSUES_DIR, byId);
  scanDir(MY_SUMMARIES_DIR, byId);

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
