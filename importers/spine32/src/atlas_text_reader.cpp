// Task 4 B5: `.atlas` 文本解析实现。
//
// 本文件的解析逻辑从 B1 的 atlas_import.cpp **原样搬移**，行为不得改变：
// B1 的 8 项测试仍然断言原有诊断码与消息。新增的只有两处 ——
//   1. 页属性 format/filter/repeat/pma 与区域 index 现在**保留在 IR 里**（原来只告警后丢弃）；
//   2. 诊断改用 AtlasTextDiagnostic，由 atlas_import.cpp 翻译回 AtlasImportDiagnostic。
//
// 依据：docs/skeleton/field-mapping.md §10；语料 1113 个单位全部为 libGDX 风格单页图集，
// 多页无真实样本，只能靠合成文本覆盖。

#include "loongbones/importers/spine32/atlas_text_reader.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>

namespace loongbones::importers::spine32 {
namespace {

constexpr std::size_t kMaxAtlasBytes = 16U * 1024U * 1024U;

struct Property {
  std::string key;
  std::string value;
  std::size_t line = 0;
  std::size_t indent = 0;
};
struct Block {
  std::string name;
  std::size_t line = 0;
  std::vector<Property> properties;
};

std::string trim(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
    return std::isspace(c) != 0;
  });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
    return std::isspace(c) != 0;
  }).base();
  if (first >= last) return {};
  return std::string(first, last);
}

void addDiag(std::vector<AtlasTextDiagnostic>& out, AtlasTextCode code,
             AtlasTextSeverity severity, std::string message, std::string location) {
  out.push_back({code, severity, std::move(message), std::move(location)});
}

bool parseInt(std::string_view text, std::int32_t& value) {
  const std::string input = trim(std::string(text));
  if (input.empty()) return false;
  try {
    std::size_t consumed = 0;
    const long long parsed = std::stoll(input, &consumed, 10);
    if (consumed != input.size() || parsed < std::numeric_limits<std::int32_t>::min() ||
        parsed > std::numeric_limits<std::int32_t>::max()) return false;
    value = static_cast<std::int32_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
}

bool parsePair(std::string_view text, std::int32_t& a, std::int32_t& b) {
  const std::size_t comma = text.find(',');
  if (comma == std::string_view::npos || text.find(',', comma + 1) != std::string_view::npos) return false;
  return parseInt(text.substr(0, comma), a) && parseInt(text.substr(comma + 1), b);
}

bool parseBlocks(std::string_view text, std::vector<Block>& blocks,
                 std::vector<AtlasTextDiagnostic>& diagnostics) {
  if (text.size() > kMaxAtlasBytes) {
    addDiag(diagnostics, AtlasTextCode::kValueInvalid, AtlasTextSeverity::kError,
            "Atlas 文本超过 16 MiB 安全上限", "atlas");
    return false;
  }
  std::istringstream stream{std::string(text)};
  std::string line;
  std::vector<std::pair<std::size_t, std::string>> lines;
  std::size_t lineNo = 0;
  auto flushBlock = [&]() -> bool {
    if (lines.empty()) return true;
    Block block;
    block.name = trim(lines.front().second);
    block.line = lines.front().first;
    if (block.name.empty() || block.name.front() == '#' || block.name.front() == ';') {
      lines.clear();
      return true;
    }
    for (std::size_t i = 1; i < lines.size(); ++i) {
      const std::string& raw = lines[i].second;
      const std::size_t indent = raw.find_first_not_of(" \t");
      const std::size_t colon = raw.find(':', indent == std::string::npos ? 0 : indent);
      if (colon == std::string_view::npos || indent == std::string::npos || colon <= indent) {
        addDiag(diagnostics, AtlasTextCode::kSyntax, AtlasTextSeverity::kError,
                "Atlas 属性格式错误", "line " + std::to_string(lines[i].first));
        return false;
      }
      Property prop{trim(raw.substr(indent, colon - indent)), trim(raw.substr(colon + 1)),
                    lines[i].first, indent};
      if (prop.key.empty() || prop.value.empty()) {
        addDiag(diagnostics, AtlasTextCode::kSyntax, AtlasTextSeverity::kError,
                "Atlas 属性名或值为空", "line " + std::to_string(lines[i].first));
        return false;
      }
      block.properties.push_back(std::move(prop));
    }
    blocks.push_back(std::move(block));
    lines.clear();
    return true;
  };
  while (std::getline(stream, line)) {
    ++lineNo;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (trim(line).empty()) {
      if (!flushBlock()) return false;
    } else {
      // In old Spine/libGDX atlases the first Region may follow page
      // properties immediately, with no separating blank line.
      const std::size_t indent = line.find_first_not_of(" \t");
      if (!lines.empty() && indent == 0 && line.find(':') == std::string::npos &&
          lines.size() > 1 && lines.front().second.find(':') == std::string::npos) {
        if (!flushBlock()) return false;
      }
      lines.emplace_back(lineNo, line);
    }
  }
  if (!flushBlock()) return false;
  return !blocks.empty();
}

}  // namespace

AtlasText readAtlasText(std::string_view text) {
  AtlasText result;
  std::vector<AtlasTextDiagnostic>& diags = result.diagnostics;

  // Some extracted Unity TextAsset files wrap the original libGDX Atlas text
  // in `string m_Script = "..."`. Treat that container as transport metadata;
  // never interpret it as Atlas page content.
  const std::string_view scriptMarker = "string m_Script = \"";
  const std::size_t marker = text.find(scriptMarker);
  if (marker != std::string_view::npos) {
    const std::size_t lineEnd = text.find('\n', marker);
    if (lineEnd == std::string_view::npos ||
        text.substr(0, marker).find("TextAsset") == std::string_view::npos) {
      addDiag(diags, AtlasTextCode::kSyntax, AtlasTextSeverity::kError,
              "Unity TextAsset Atlas 包装头格式错误", "atlas");
      return result;
    }
    text.remove_prefix(lineEnd + 1);
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' ')) {
      text.remove_suffix(1);
    }
    if (text.empty() || text.back() != '"') {
      addDiag(diags, AtlasTextCode::kSyntax, AtlasTextSeverity::kError,
              "Unity TextAsset Atlas 缺少结束引号", "atlas");
      return result;
    }
    text.remove_suffix(1);
  }

  std::vector<Block> blocks;
  if (!parseBlocks(text, blocks, diags)) {
    if (diags.empty()) addDiag(diags, AtlasTextCode::kSyntax, AtlasTextSeverity::kError,
                               "Atlas 文本为空", "atlas");
    return result;
  }

  std::vector<SourceAtlasPage>& pages = result.pages;
  SourceAtlasPage* page = nullptr;
  for (const Block& block : blocks) {
    if (block.properties.empty()) {
      addDiag(diags, AtlasTextCode::kSyntax, AtlasTextSeverity::kError,
              "页/区域块缺少属性", "line " + std::to_string(block.line));
      return result;
    }
    const bool isPage = block.properties.front().indent == 0;
    if (isPage) {
      SourceAtlasPage next;
      next.file = block.name;
      next.line = block.line;
      for (const Property& prop : block.properties) {
        if (prop.indent != 0) {
          addDiag(diags, AtlasTextCode::kSyntax, AtlasTextSeverity::kError,
                  "页属性缩进不一致", "line " + std::to_string(prop.line));
          return result;
        }
        if (prop.key == "size") {
          if (!parsePair(prop.value, next.width, next.height)) {
            addDiag(diags, AtlasTextCode::kValueInvalid, AtlasTextSeverity::kError,
                    "页 size 必须是两个 int32", "line " + std::to_string(prop.line));
            return result;
          }
        } else if (prop.key == "format") {
          next.properties.format = prop.value;
          addDiag(diags, AtlasTextCode::kFeatureUnsupported, AtlasTextSeverity::kWarning,
                  "页属性未进入当前 Domain 模型，将不会保存: " + prop.key,
                  "line " + std::to_string(prop.line));
        } else if (prop.key == "filter") {
          next.properties.filter = prop.value;
          addDiag(diags, AtlasTextCode::kFeatureUnsupported, AtlasTextSeverity::kWarning,
                  "页属性未进入当前 Domain 模型，将不会保存: " + prop.key,
                  "line " + std::to_string(prop.line));
        } else if (prop.key == "repeat") {
          next.properties.repeat = prop.value;
          addDiag(diags, AtlasTextCode::kFeatureUnsupported, AtlasTextSeverity::kWarning,
                  "页属性未进入当前 Domain 模型，将不会保存: " + prop.key,
                  "line " + std::to_string(prop.line));
        } else if (prop.key == "pma") {
          next.properties.pma = (prop.value == "true");
          addDiag(diags, AtlasTextCode::kFeatureUnsupported, AtlasTextSeverity::kWarning,
                  "页属性未进入当前 Domain 模型，将不会保存: " + prop.key,
                  "line " + std::to_string(prop.line));
        } else {
          addDiag(diags, AtlasTextCode::kFeatureUnsupported, AtlasTextSeverity::kWarning,
                  "未识别的页属性将不会保存: " + prop.key,
                  "line " + std::to_string(prop.line));
        }
      }
      if (next.width <= 0 || next.height <= 0) {
        addDiag(diags, AtlasTextCode::kValueInvalid, AtlasTextSeverity::kError,
                "页尺寸必须为正数，且必须包含 size", "line " + std::to_string(block.line));
        return result;
      }
      pages.push_back(std::move(next));
      page = &pages.back();
      continue;
    }
    if (page == nullptr) {
      addDiag(diags, AtlasTextCode::kSyntax, AtlasTextSeverity::kError,
              "区域出现在任何图集页之前", "line " + std::to_string(block.line));
      return result;
    }
    SourceAtlasRegion region;
    region.name = block.name;
    region.line = block.line;
    bool hasXY = false, hasSize = false;
    for (const Property& prop : block.properties) {
      if (prop.indent == 0) {
        addDiag(diags, AtlasTextCode::kSyntax, AtlasTextSeverity::kError,
                "区域属性必须缩进", "line " + std::to_string(prop.line));
        return result;
      }
      if (prop.key == "xy") {
        hasXY = parsePair(prop.value, region.x, region.y);
        if (!hasXY) {
          addDiag(diags, AtlasTextCode::kValueInvalid, AtlasTextSeverity::kError,
                  "区域 xy 必须是两个 int32", "line " + std::to_string(prop.line));
          return result;
        }
      } else if (prop.key == "size") {
        hasSize = parsePair(prop.value, region.width, region.height);
        if (!hasSize) {
          addDiag(diags, AtlasTextCode::kValueInvalid, AtlasTextSeverity::kError,
                  "区域 size 必须是两个 int32", "line " + std::to_string(prop.line));
          return result;
        }
      } else if (prop.key == "orig") {
        region.hasOriginal = true;
        if (!parsePair(prop.value, region.originalWidth, region.originalHeight)) {
          addDiag(diags, AtlasTextCode::kValueInvalid, AtlasTextSeverity::kError,
                  "区域 orig 必须是两个 int32", "line " + std::to_string(prop.line));
          return result;
        }
      } else if (prop.key == "offset") {
        if (!parsePair(prop.value, region.offsetX, region.offsetY)) {
          addDiag(diags, AtlasTextCode::kValueInvalid, AtlasTextSeverity::kError,
                  "区域 offset 必须是两个 int32", "line " + std::to_string(prop.line));
          return result;
        }
      } else if (prop.key == "rotate") {
        if (prop.value == "true") region.rotation = RegionRotation::kDeg90;
        else if (prop.value == "false") region.rotation = RegionRotation::kNone;
        else {
          addDiag(diags, AtlasTextCode::kValueInvalid, AtlasTextSeverity::kError,
                  "当前只支持 rotate: true/false", "line " + std::to_string(prop.line));
          return result;
        }
      } else if (prop.key == "index") {
        std::int32_t index = 0;
        if (!parseInt(prop.value, index)) {
          addDiag(diags, AtlasTextCode::kValueInvalid, AtlasTextSeverity::kError,
                  "区域 index 必须是 int32", "line " + std::to_string(prop.line));
          return result;
        }
        region.index = index;
        if (index != -1) addDiag(diags, AtlasTextCode::kFeatureUnsupported,
                                 AtlasTextSeverity::kWarning,
                                 "Atlas sequence/index 未进入当前 Domain 模型", "line " + std::to_string(prop.line));
      } else {
        addDiag(diags, AtlasTextCode::kFeatureUnsupported, AtlasTextSeverity::kWarning,
                "未识别或未支持的区域属性将不会保存: " + prop.key,
                "line " + std::to_string(prop.line));
      }
    }
    if (!hasXY || !hasSize) {
      addDiag(diags, AtlasTextCode::kValueInvalid, AtlasTextSeverity::kError,
              "区域必须包含 xy 和 size", "line " + std::to_string(block.line));
      return result;
    }
    if (!region.hasOriginal) {
      region.originalWidth = region.width;
      region.originalHeight = region.height;
    }
    if (region.width <= 0 || region.height <= 0 || region.originalWidth <= 0 || region.originalHeight <= 0) {
      addDiag(diags, AtlasTextCode::kValueInvalid, AtlasTextSeverity::kError,
              "区域尺寸必须为正数", "line " + std::to_string(block.line));
      return result;
    }
    region.trimmedWidth = region.width;
    region.trimmedHeight = region.height;
    page->regions.push_back(std::move(region));
  }
  if (pages.empty()) {
    addDiag(diags, AtlasTextCode::kSyntax, AtlasTextSeverity::kError,
            "Atlas 未包含任何图集页", "atlas");
    return result;
  }
  for (const SourceAtlasPage& p : pages) {
    if (p.regions.empty()) {
      addDiag(diags, AtlasTextCode::kSyntax, AtlasTextSeverity::kError,
              "空图集页不支持", "line " + std::to_string(p.line));
      return result;
    }
  }
  result.valid = true;
  return result;
}

const char* toString(AtlasTextCode code) noexcept {
  switch (code) {
    case AtlasTextCode::kNone: return "NONE";
    case AtlasTextCode::kSyntax: return "ATLAS_SYNTAX";
    case AtlasTextCode::kValueInvalid: return "ATLAS_VALUE_INVALID";
    case AtlasTextCode::kFeatureUnsupported: return "ATLAS_FEATURE_UNSUPPORTED";
  }
  return "UNKNOWN";
}

}  // namespace loongbones::importers::spine32
