#include "loongbones/importers/api/path_util.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace loongbones::importers::api {
namespace {

std::vector<std::string> splitSegments(std::string_view text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (true) {
    const auto slash = text.find('/', start);
    const auto seg = text.substr(start, slash == std::string_view::npos
                                            ? std::string_view::npos
                                            : slash - start);
    out.emplace_back(seg);
    if (slash == std::string_view::npos) break;
    start = slash + 1;
  }
  return out;
}

}  // namespace

bool isSafeRelativePath(std::string_view relative) noexcept {
  if (relative.empty()) return false;
  if (relative.find('\0') != std::string_view::npos) return false;
  // Windows 风格：反斜杠与盘符绝不接受（端口一致性 + 规避 `C:\` 绝对路径）。
  if (relative.find('\\') != std::string_view::npos) return false;
  if (relative.size() >= 2 && relative[1] == ':') return false;
  if (relative.front() == '/') return false;
  for (const std::string& seg : splitSegments(relative)) {
    if (seg.empty() || seg == "." || seg == "..") return false;
  }
  return true;
}

std::string normalizedRelativePath(std::string_view relative) {
  std::vector<std::string> parts;
  for (const std::string& seg : splitSegments(relative)) {
    if (seg.empty() || seg == ".") continue;
    if (seg == "..") {
      if (parts.empty()) return {};  // 试图越出根部
      parts.pop_back();
      continue;
    }
    parts.push_back(seg);
  }
  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i != 0) out += '/';
    out += parts[i];
  }
  return out;
}

bool isPathWithin(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  if (root.empty() || candidate.empty()) return false;
  const auto mism = std::mismatch(root.begin(), root.end(), candidate.begin(), candidate.end());
  return mism.first == root.end();
}

}  // namespace loongbones::importers::api
