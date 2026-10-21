// Task 4 B5: Spine / libGDX `.atlas` 文本 → 插件私有 IR。
//
// 纪律（Task4_Implementation_Plan.md B5）：
//   · 只做**只读解析**，产出源格式对象；不建 Domain 对象、不碰文件系统、不解码像素。
//   · 页块声明的 `size` 是**页坐标尺寸**，不等于伴随 PNG 的 IHDR 像素尺寸
//     （field-mapping §10.2：1107/1111 不一致）。两者分别保存，不得互相覆盖。
//   · `format` / `filter` / `repeat` / `pma` 在当前 Domain 模型里没有落点 ——
//     本 Reader **解析并保留**（供 B5 报告），但 Domain 不承载，映射时给 warning。
//   · 一颗区域矩形规则：沿用 `AtlasRegion` 的 packed/rotation/original/trimmed/offset，
//     不另造第二套（B5 方案第 3 条）。
//
// 本文件从 B1 的 atlas_import.cpp 提取而来：原来那套解析器是文件内私有的，B5 的
// Mapper 需要独立复用与单独断言，故提出为公开 API。B1 行为不变。

#ifndef LOONGBONES_IMPORTERS_SPINE32_ATLAS_TEXT_READER_HPP
#define LOONGBONES_IMPORTERS_SPINE32_ATLAS_TEXT_READER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "loongbones/domain/atlas.hpp"

namespace loongbones::importers::spine32 {

enum class AtlasTextCode {
  kNone,
  kSyntax,
  kValueInvalid,
  kFeatureUnsupported,
};

enum class AtlasTextSeverity { kWarning, kError };

struct AtlasTextDiagnostic {
  AtlasTextCode code = AtlasTextCode::kNone;
  AtlasTextSeverity severity = AtlasTextSeverity::kError;
  std::string message;
  std::string sourceLocation;
};

/// 一个图集区域（源格式观察值）。
struct SourceAtlasRegion {
  std::string name;
  std::size_t line = 0;
  /// 页内打包矩形（左上角 + 尺寸），单位=页坐标像素。
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
  /// `orig`：未裁剪未打包的原始尺寸。缺省时回退为 size。
  std::int32_t originalWidth = 0;
  std::int32_t originalHeight = 0;
  bool hasOriginal = false;
  /// 裁切后尺寸。libGDX 图集文本不单独写 `trim` —— `size` 就是裁切后的尺寸。
  /// 方向与 original 一致，**不受打包旋转影响**。
  std::int32_t trimmedWidth = 0;
  std::int32_t trimmedHeight = 0;
  /// `offset`：trim 偏移；方向与 original 一致，不受打包旋转影响。
  std::int32_t offsetX = 0;
  std::int32_t offsetY = 0;
  /// `rotate: true` → 90°。图集文本只表达 0/90，180/270 不由此产生。
  RegionRotation rotation = RegionRotation::kNone;
  /// `index`：序列帧序号（-1 = 无序列）。语料 0 例，仅解析记录，不进 Domain。
  std::optional<std::int32_t> index;
};

/// 页头声明、但当前 Domain 模型不承载的属性。B5 解析并报告，不落 canonical。
struct SourceAtlasPageProperties {
  std::optional<std::string> format;
  std::optional<std::string> filter;
  std::optional<std::string> repeat;
  std::optional<bool> pma;

  [[nodiscard]] bool any() const noexcept {
    return format.has_value() || filter.has_value() || repeat.has_value() || pma.has_value();
  }
};

struct SourceAtlasPage {
  /// 页块名**就是**页头 `file`（图片文件名）。
  std::string file;
  std::size_t line = 0;
  /// 页**声明**的坐标尺寸。与 PNG IHDR 像素尺寸分离保存（B5 验收项）。
  std::int32_t width = 0;
  std::int32_t height = 0;
  SourceAtlasPageProperties properties;
  std::vector<SourceAtlasRegion> regions;
};

struct AtlasText {
  std::vector<SourceAtlasPage> pages;
  std::vector<AtlasTextDiagnostic> diagnostics;
  bool valid = false;
};

/// 只读解析一份图集文本。支持多页（语料 0 例，靠合成文本覆盖）。
///
/// 失败时 `valid == false` 且 diagnostics 至少一条 error；成功也可能带 warning
/// （不支持/未识别的页与区域属性）。解析出的 region **不做**页边界校验 ——
/// 那是 Domain 层 `SKELETON_REGION_OUT_OF_PAGE` 一类判据的事。
AtlasText readAtlasText(std::string_view text);

const char* toString(AtlasTextCode code) noexcept;

}  // namespace loongbones::importers::spine32

#endif  // LOONGBONES_IMPORTERS_SPINE32_ATLAS_TEXT_READER_HPP
