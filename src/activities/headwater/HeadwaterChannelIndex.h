#pragma once
#include <string>
#include <vector>

namespace headwater {

struct ChannelEntry {
  std::string path;        // full SD path to the EPUB (daily issue or saved summary)
  std::string anchor;      // matches the EPUB TOC href, e.g. "summary-abc123.xhtml"
  std::string videoTitle;
  std::string date;        // ISO date string — used for sort (newest first)
};

struct Channel {
  std::string channelId;
  std::string displayName;
  std::vector<ChannelEntry> entries;  // newest-first by date
};

struct ChannelIndex {
  std::vector<Channel> channels;  // alphabetical by displayName
};

// Scan /Headwater (daily issues) and /Headwater/My Summaries (sideloaded
// summaries), parse each EPUB's embedded manifest, and build an in-memory channel
// index grouped by channelId. Returns false if no manifests were found.
bool buildChannelIndex(ChannelIndex& out);

}  // namespace headwater
