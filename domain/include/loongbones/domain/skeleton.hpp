// 骨架数据模型（Task 4 B2；schema 契约见 docs/format/native-project-v2-skeleton.md）
//
// 纪律（与 atlas.hpp / node.hpp 一致）：
//   · 纯数据：不解码 Spine 二进制、不解析图集文本、不做求值、不碰文件系统。
//   · 身份与实例分离：Bone / Slot / Skin / SkinEntry 各自持有稳定 ID；
//     所有跨对象引用一律用 ID，不用名称、不用数组下标。
//   · Skin **只嵌套于 Skeleton**；Project 顶层没有 skins[]（D27 / v2 schema §1）。
//   · Bone 不是 SceneNode：本类型不继承、不依赖 node.hpp 的层级语义。
//   · 数值字段（x/y/rotation/scale/shear/length/width/height/color）必须有限；
//     NaN / Inf 由 Project::validate() 拦截（v2 schema §9）。
//   · SourceRef 是**整个可选**对象：导入时提供，原生创建时省略（B0 退回补项）。
//
// 本文件只实现 v2 schema 首批冻结类型。Mesh / 加权 mesh / linked mesh、
// IK / transform 约束、动画**不在**本任务范围（v2 schema §10）。

#ifndef LOONGBONES_DOMAIN_SKELETON_HPP
#define LOONGBONES_DOMAIN_SKELETON_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "loongbones/domain/atlas.hpp"
#include "loongbones/domain/diagnostics.hpp"
#include "loongbones/domain/ids.hpp"

namespace loongbones {

using SkeletonId = Id;
using BoneId = Id;
using SlotId = Id;
using SkinId = Id;
using SkinEntryId = Id;

/// 各自种类的 ID 构造器，保证强类型区分（不会把 NodeId 当成 BoneId 用）。
inline SkeletonId newSkeletonId() { return Id::generate(IdKind::Skeleton); }
inline BoneId newBoneId() { return Id::generate(IdKind::Bone); }
inline SlotId newSlotId() { return Id::generate(IdKind::Slot); }
inline SkinId newSkinId() { return Id::generate(IdKind::Skin); }
inline SkinEntryId newSkinEntryId() { return Id::generate(IdKind::SkinEntry); }

/// 插槽混合模式。枚举字符串契约见 v2 schema §3.3（Format 层负责编解码）。
enum class BlendMode {
  kNormal,
  kAdditive,
  kMultiply,
  kScreen,
};

inline bool isValidBlendMode(BlendMode m) noexcept {
  switch (m) {
    case BlendMode::kNormal:
    case BlendMode::kAdditive:
    case BlendMode::kMultiply:
    case BlendMode::kScreen:
      return true;
  }
  return false;
}

/// 颜色 / 透明度。取值 0..1（ADR-0003 C8）。
///
/// 范围校验（0..1）属 Format 层（v2 schema §3.3：越界报 kJsonTypeMismatch）；
/// Domain 只保证**有限性** —— NaN/Inf 会污染求值与序列化，是领域级不变量。
struct ColorF {
  double r = 1.0;
  double g = 1.0;
  double b = 1.0;
  double a = 1.0;

  [[nodiscard]] bool isFinite() const noexcept;

  friend bool operator==(const ColorF& x, const ColorF& y) noexcept {
    return x.r == y.r && x.g == y.g && x.b == y.b && x.a == y.a;
  }
  friend bool operator!=(const ColorF& x, const ColorF& y) noexcept { return !(x == y); }
};

/// 骨骼：骨架内部的层级变换载体。**不是 SceneNode**（D27）。
///
/// parentId 为空（默认构造的 Id）表示根骨骼；否则必须指向同一骨架内
/// **排序在前**的骨骼（禁止前向引用与成环，v2 schema §3.2）。
struct Bone {
  BoneId id;
  std::string name;
  BoneId parent;  ///< isNone() = 根
  double x = 0.0;
  double y = 0.0;
  double rotationDeg = 0.0;
  double scaleX = 1.0;
  double scaleY = 1.0;
  double shearXDeg = 0.0;
  double shearYDeg = 0.0;
  /// 3.2 用两个独立布尔，**不是** 3.6+ 的 transform 枚举（domain-rfc §6.4）。
  bool inheritRotation = true;
  bool inheritScale = true;
  /// 仅编辑器可视辅助，不影响求值。
  double length = 0.0;

  [[nodiscard]] bool isRoot() const noexcept { return parent.isNone(); }
};

/// 插槽：骨架内部的绘制槽位，绑定一个骨骼。
/// 绘制顺序由 Skeleton.drawOrder 唯一确定，slot 自身**不带** z 值。
struct Slot {
  SlotId id;
  std::string name;
  BoneId boneId;
  ColorF color;
  /// default 皮肤下的当前附件名；空 = 不显示。
  std::optional<std::string> attachmentName;
  BlendMode blendMode = BlendMode::kNormal;
};

/// Region 附件：用一个 AtlasRegion 在骨骼局部空间绘制一张图。
/// type 判别字段（"region"）由 Format 层承载；Domain 侧只有这一种附件类型。
struct RegionAttachment {
  AtlasRegionId regionId;
  double x = 0.0;
  double y = 0.0;
  double rotationDeg = 0.0;
  double scaleX = 1.0;
  double scaleY = 1.0;
  double width = 0.0;
  double height = 0.0;
  ColorF color;
};

/// 皮肤条目：(Slot, 附件名) → 附件。
///
/// **必须有稳定 ID**：entries 是可重排数组；无 ID 时未知字段锚点会退化成
/// 数组下标，违反 D22（v2 schema §5.3）。
struct SkinEntry {
  SkinEntryId id;
  SlotId slotId;
  std::string attachmentName;
  RegionAttachment attachment;
};

/// 皮肤：只嵌套于 Skeleton，Project 顶层无此集合（D27）。
struct Skin {
  SkinId id;
  std::string name;
  std::vector<SkinEntry> entries;
};

/// 来源引用元数据。**整个对象可选**（B0 退回补项，2026-10-20）：
/// 导入时提供，原生创建的骨架省略。不得为"必须有个值"而伪造来源信息。
///
/// 两个 hash 字段来源与算法都不同，不得混称：
///   · sourceHeaderHash —— 源自带头字段原文，无算法，可选；
///   · contentHash      —— 宿主/插件对源文件全文计算的 SHA-256，必需。
struct SourceRef {
  std::string formatId;
  std::string formatVersion;
  std::optional<std::string> sourceHeaderHash;
  std::string contentHash;
  std::optional<std::string> sourceUnit;
  std::string importedByPluginId;
  std::int64_t importedByPluginVersion = 0;
};

/// 骨架：Project 级资产，可被未来的专用骨架实例类型引用；**不入 Scene**（D27）。
struct Skeleton {
  SkeletonId id;
  std::string name;
  /// 引用的图集（ID），用于附件 → region 解析。
  std::vector<AtlasId> atlasIds;
  /// 骨骼 ID 的规范顺序；与 bones[].id 互为排列。
  std::vector<BoneId> boneOrder;
  std::vector<Bone> bones;
  /// Slot ID 的有序数组 = 绘制顺序（后者绘于上层）；与 slots[].id 互为排列。
  std::vector<SlotId> drawOrder;
  std::vector<Slot> slots;
  std::vector<Skin> skins;
  std::optional<SourceRef> sourceRef;
};

struct ProjectData;  ///< project.hpp；跨集合引用闭合校验需要工程上下文

/// 单个骨架的全部违规诊断（空 = 通过）。
///
/// 骨架 ID 自身的形态 / 种类 / 跨骨架唯一性由 Project::validate() 统一处理，
/// 不在这里重复。所有判据集中在本函数，供写 API 与整体校验共用。
[[nodiscard]] std::vector<Diagnostic> validateSkeleton(const Skeleton& skeleton,
                                                       const ProjectData& project);

}  // namespace loongbones

#endif  // LOONGBONES_DOMAIN_SKELETON_HPP
