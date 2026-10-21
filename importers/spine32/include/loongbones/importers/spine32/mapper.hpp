// Task 4 B5: Source（Spine 3.2.01 私有 IR）→ canonical（Domain `Skeleton`）映射。
//
// 纪律（Task4_Implementation_Plan.md B5）：
//   · 所有持久化 ID **只能**由宿主 `IIdFactory` 发放；Mapper 自己不生成、不复用源下标。
//     源下标 → ID 的解析发生在本次导入的**局部映射表**里，不写入任何产物。
//   · 附件 region 查找键 = **`path ?? name`**（field-mapping §10.1，语料命中 47,705/47,709）。
//     精确匹配优先；未命中可按 `trim()` 重试并记**降级 warning**（T-B3）；
//     仍未命中是 **error**，拒绝整个候选 —— 静默丢弃等于运行时缺件。
//   · B4 只遍历并计数的能力（mesh 族 / boundingbox / 动画时间线 / IK / transform / 事件）
//     在这里**只产出计数与诊断，不建半成品 Domain 对象**。
//   · 本文件只依赖插件私有 IR 与 Domain 公共类型；不解析二进制、不碰文件系统。
//
// 边界提醒：这里的「能映射」不等于「能渲染」—— Domain `RegionAttachment` 只承载
// region，网格/蒙皮/动画仍不在 canonical 范围内（v2 schema §10）。

#ifndef LOONGBONES_IMPORTERS_SPINE32_MAPPER_HPP
#define LOONGBONES_IMPORTERS_SPINE32_MAPPER_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "loongbones/domain/atlas.hpp"
#include "loongbones/domain/ids.hpp"
#include "loongbones/domain/skeleton.hpp"
#include "loongbones/importers/spine32/atlas_import.hpp"
#include "loongbones/importers/spine32/source_model.hpp"

namespace loongbones::importers::spine32 {

enum class MapperCode {
  kNone,
  kUnsupportedVersion,
  kBoneParentOutOfRange,
  kSlotBoneOutOfRange,
  kSkinSlotOutOfRange,
  kNonFiniteFloat,
  kAttachmentRegionUnresolved,
  kAttachmentRegionDegraded,
  kUnsupportedCapability,
};

enum class MapperSeverity { kWarning, kError };

struct MapperDiagnostic {
  MapperCode code = MapperCode::kNone;
  MapperSeverity severity = MapperSeverity::kError;
  std::string message;
  /// 源内位置，例如 `skins[0]/slots[3]/attachments[1]`。
  std::string sourceLocation;
};

/// 图集 region 查找。返回 `isNone()` 的 Id 表示未命中。
class IAtlasRegionLookup {
 public:
  virtual ~IAtlasRegionLookup() = default;
  virtual AtlasRegionId find(std::string_view regionKey) const = 0;
};

/// 对若干**已存在的** canonical Atlas 建名字索引。
/// 同名 region 取先出现者（`Atlas` 数组顺序），并记录一次 warning 级的重复提示由调用方决定。
class AtlasRegionIndex final : public IAtlasRegionLookup {
 public:
  explicit AtlasRegionIndex(const std::vector<Atlas>& atlases);
  AtlasRegionId find(std::string_view regionKey) const override;
  [[nodiscard]] std::size_t size() const noexcept { return byName_.size(); }

 private:
  std::unordered_map<std::string, AtlasRegionId> byName_;
};

struct MapperOptions {
  std::string skeletonName;
  /// 该骨架引用的图集（ID）。附件 → region 的解析范围就是这些图集。
  std::vector<AtlasId> atlasIds;
  std::optional<SourceRef> sourceRef;
  /// T-B3：精确匹配未命中时，是否按 `trim()` 后的键重试；命中则记降级 warning。
  /// 关闭后未命中一律 error。
  bool allowTrailingSpaceFallback = true;
};

struct MapResult {
  Skeleton skeleton;
  std::vector<MapperDiagnostic> diagnostics;
  /// 源里存在、但本阶段不转换的能力（原样带出，供报告）。不对应任何 Domain 对象。
  UnsupportedCounts unsupported;
  std::size_t regionAttachmentCount = 0;
  /// 无 error 级诊断时为 true。warning 不阻止提交（需宿主显式确认）。
  bool valid = false;
};

/// 纯函数式映射：不改输入，不碰 Project，不写盘。
MapResult mapSkeletonToCanonical(const SourceSkeleton& source, const MapperOptions& options,
                                 const IAtlasRegionLookup& regions, IIdFactory& ids);

const char* toString(MapperCode code) noexcept;

}  // namespace loongbones::importers::spine32

#endif  // LOONGBONES_IMPORTERS_SPINE32_MAPPER_HPP
