#include "HeadwaterDeleted.h"

#include <HalStorage.h>

#include "HeadwaterPaths.h"

namespace headwater {
namespace {
// Hidden sidecar in the issues dir. Not an .epub, so the issue/channel scans
// (which filter on extension) never surface it.
constexpr char DELETED_LIST[] = "/Headwater/.deleted";
}  // namespace

std::set<std::string> loadDeletedIssues() {
  std::set<std::string> out;
  if (!Storage.exists(DELETED_LIST)) return out;
  const String content = Storage.readFile(DELETED_LIST);
  std::string line;
  for (size_t i = 0; i < content.length(); ++i) {
    const char c = content[i];
    if (c == '\n' || c == '\r') {
      if (!line.empty()) out.insert(line);
      line.clear();
    } else {
      line += c;
    }
  }
  if (!line.empty()) out.insert(line);
  return out;
}

void saveDeletedIssues(const std::set<std::string>& names) {
  String content;
  for (const auto& n : names) {
    content += n.c_str();
    content += '\n';
  }
  Storage.writeFile(DELETED_LIST, content);
}

void markIssueDeleted(const std::string& fileName) {
  auto names = loadDeletedIssues();
  if (names.insert(fileName).second) saveDeletedIssues(names);
}

}  // namespace headwater
