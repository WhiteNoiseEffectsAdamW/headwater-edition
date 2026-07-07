#pragma once
#include <set>
#include <string>

namespace headwater {

// Tiny persisted string-set store used for the Channels curation state: read
// videoIds, hidden videoIds, and muted channelIds. Each lives in its own hidden
// sidecar file under /Headwater (see HeadwaterPaths). Newline-separated; the
// caller owns the in-memory set and writes it back after any change.

std::set<std::string> loadIdSet(const char* path);
void saveIdSet(const char* path, const std::set<std::string>& ids);

}  // namespace headwater
