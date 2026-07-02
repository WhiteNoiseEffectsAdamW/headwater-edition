#pragma once
#include <set>
#include <string>

namespace headwater {

// Tombstones for issues the user deleted on-device, keyed by EPUB file name
// (e.g. "Headwater - Headwater Daily 2026-06-29.epub"). Sync idempotency is
// filename-based, so without this a deleted issue that is still inside the
// rolling feed window would just be re-downloaded on the next sync. The list is
// pruned to the current feed each sync, so it stays bounded to the feed size.

// Read the tombstone set (empty if the file is absent).
std::set<std::string> loadDeletedIssues();

// Record one deleted issue (no-op if already present).
void markIssueDeleted(const std::string& fileName);

// Rewrite the tombstone set (used to prune entries that aged out of the feed).
void saveDeletedIssues(const std::set<std::string>& names);

}  // namespace headwater
