#include "HeadwaterIdSet.h"

#include <HalStorage.h>

namespace headwater {

std::set<std::string> loadIdSet(const char* path) {
  std::set<std::string> out;
  if (!Storage.exists(path)) return out;
  const String content = Storage.readFile(path);
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

void saveIdSet(const char* path, const std::set<std::string>& ids) {
  String content;
  for (const auto& id : ids) {
    content += id.c_str();
    content += '\n';
  }
  Storage.writeFile(path, content);
}

}  // namespace headwater
