// Task 4 B5: Source → canonical 映射实现。
//
// 位置字符串的形态固定为源内路径（如 `bones[3]`、`skins[0]/slots[2]/attachments[0]`），
// 与 B4 Reader 的 `section` 命名保持同一套风格，便于诊断串联。

#include "loongbones/importers/spine32/mapper.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace loongbones::importers::spine32 {
namespace {

void addDiag(std::vector<MapperDiagnostic>& out, MapperCode code, MapperSeverity severity,
             std::string message, std::string location) {
  out.push_back({code, severity, std::move(message), std::move(location)});
}

/// 源 `0xRRGGBBAA` → 0..1 浮点（ADR-0003 C8）。只读分量，不做色彩空间换算。
ColorF colorFromRgba(std::uint32_t rgba) noexcept {
  constexpr double kInv = 1.0 / 255.0;
  ColorF c;
  c.r = static_cast<double>((rgba >> 24U) & 0xFFU) * kInv;
  c.g = static_cast<double>((rgba >> 16U) & 0xFFU) * kInv;
  c.b = static_cast<double>((rgba >> 8U) & 0xFFU) * kInv;
  c.a = static_cast<double>(rgba & 0xFFU) * kInv;
  return c;
}

BlendMode blendFromSource(int blend) noexcept {
  switch (blend) {
    case 1: return BlendMode::kAdditive;
    case 2: return BlendMode::kMultiply;
    case 3: return BlendMode::kScreen;
    default: return BlendMode::kNormal;
  }
}

std::string trimCopy(std::string_view text) {
  const auto isSpace = [](char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
  };
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end && isSpace(text[begin])) ++begin;
  while (end > begin && isSpace(text[end - 1])) --end;
  return std::string(text.substr(begin, end - begin));
}

/// 报告一类"源里有、本阶段不转换"的能力：只给计数与**所在段**，不建 Domain 对象。
///
/// 位置只有段名而非字节偏移 —— B4 的 `UnsupportedCounts` 只记了数量，没有记录
/// 首个出现位置；这是 B4 的已知缺口，此处如实标注，不伪造精确位置。
void reportUnsupported(std::vector<MapperDiagnostic>& out, MapperCode code, const char* what,
                       std::size_t count, const char* section) {
  if (count == 0) return;
  addDiag(out, code, MapperSeverity::kWarning,
          std::string(what) + " 共 " + std::to_string(count) +
              " 项：源里存在但本阶段不转换，不生成任何 Domain 对象",
          section);
}

}  // namespace

AtlasRegionIndex::AtlasRegionIndex(const std::vector<Atlas>& atlases) {
  for (const Atlas& atlas : atlases) {
    for (const AtlasRegion& region : atlas.regions) {
      // 先出现者优先：同名 region 在多页/多图集时以第一条为准，不覆盖、不报错。
      byName_.emplace(region.name, region.id);
    }
  }
}

AtlasRegionId AtlasRegionIndex::find(std::string_view regionKey) const {
  const auto it = byName_.find(std::string(regionKey));
  if (it == byName_.end()) return AtlasRegionId{};
  return it->second;
}

MapResult mapSkeletonToCanonical(const SourceSkeleton& source, const MapperOptions& options,
                                 const IAtlasRegionLookup& regions, IIdFactory& ids) {
  MapResult result;
  result.unsupported = source.unsupported;
  std::vector<MapperDiagnostic>& diags = result.diagnostics;

  if (source.version != kAcceptedVersion) {
    addDiag(diags, MapperCode::kUnsupportedVersion, MapperSeverity::kError,
            "源版本不是受支持的 " + std::string(kAcceptedVersion) + "：" + source.version,
            "header/version");
    return result;
  }

  Skeleton skeleton;
  skeleton.id = ids.create(IdKind::Skeleton);
  skeleton.name = options.skeletonName;
  skeleton.atlasIds = options.atlasIds;
  skeleton.sourceRef = options.sourceRef;

  // ---- 骨骼：源下标 → BoneId；父索引必须在**自身之前**（禁止前向引用/自环） ----
  std::vector<BoneId> boneIds;
  boneIds.reserve(source.bones.size());
  for (std::size_t i = 0; i < source.bones.size(); ++i) {
    const SourceBone& src = source.bones[i];
    const std::string where = "bones[" + std::to_string(i) + "]";
    if (!std::isfinite(src.rotationDeg) || !std::isfinite(src.x) || !std::isfinite(src.y) ||
        !std::isfinite(src.scaleX) || !std::isfinite(src.scaleY) ||
        !std::isfinite(src.shearXDeg) || !std::isfinite(src.shearYDeg) ||
        !std::isfinite(src.length)) {
      addDiag(diags, MapperCode::kNonFiniteFloat, MapperSeverity::kError,
              "骨骼变换出现非有限数值（NaN / ±Inf）", where);
      return result;
    }
    Bone bone;
    bone.id = ids.create(IdKind::Bone);
    bone.name = src.name;
    if (src.parentIndex >= 0) {
      if (static_cast<std::size_t>(src.parentIndex) >= i) {
        addDiag(diags, MapperCode::kBoneParentOutOfRange, MapperSeverity::kError,
                "父骨骼下标必须指向排序在前的骨骼（禁止前向引用与自环）：" +
                    std::to_string(src.parentIndex),
                where);
        return result;
      }
      bone.parent = boneIds[static_cast<std::size_t>(src.parentIndex)];
    }
    bone.x = src.x;
    bone.y = src.y;
    bone.rotationDeg = src.rotationDeg;
    bone.scaleX = src.scaleX;
    bone.scaleY = src.scaleY;
    bone.shearXDeg = src.shearXDeg;
    bone.shearYDeg = src.shearYDeg;
    bone.inheritRotation = src.inheritRotation;
    bone.inheritScale = src.inheritScale;
    bone.length = src.length;
    boneIds.push_back(bone.id);
    skeleton.bones.push_back(std::move(bone));
    skeleton.boneOrder.push_back(boneIds.back());
  }

  // ---- 插槽：数组顺序即绘制顺序 ----
  for (std::size_t i = 0; i < source.slots.size(); ++i) {
    const SourceSlot& src = source.slots[i];
    const std::string where = "slots[" + std::to_string(i) + "]";
    if (src.boneIndex < 0 || static_cast<std::size_t>(src.boneIndex) >= boneIds.size()) {
      addDiag(diags, MapperCode::kSlotBoneOutOfRange, MapperSeverity::kError,
              "插槽引用的骨骼下标越界：" + std::to_string(src.boneIndex), where);
      return result;
    }
    Slot slot;
    slot.id = ids.create(IdKind::Slot);
    slot.name = src.name;
    slot.boneId = boneIds[static_cast<std::size_t>(src.boneIndex)];
    slot.color = colorFromRgba(src.colorRgba);
    slot.attachmentName = src.attachmentName;
    slot.blendMode = blendFromSource(src.blend);
    skeleton.slots.push_back(std::move(slot));
    skeleton.drawOrder.push_back(skeleton.slots.back().id);
  }

  // ---- 皮肤：只有 region 附件（typeByte == 0）进入 canonical ----
  for (std::size_t skinIndex = 0; skinIndex < source.skins.size(); ++skinIndex) {
    const SourceSkin& srcSkin = source.skins[skinIndex];
    Skin skin;
    skin.id = ids.create(IdKind::Skin);
    skin.name = srcSkin.name.value_or("default");
    const std::string skinWhere = "skins[" + std::to_string(skinIndex) + "]";

    for (std::size_t slotIndex = 0; slotIndex < srcSkin.slots.size(); ++slotIndex) {
      const SourceSkinSlot& srcSlot = srcSkin.slots[slotIndex];
      const std::string slotWhere =
          skinWhere + "/slots[" + std::to_string(slotIndex) + "]";
      if (srcSlot.slotIndex < 0 ||
          static_cast<std::size_t>(srcSlot.slotIndex) >= skeleton.slots.size()) {
        addDiag(diags, MapperCode::kSkinSlotOutOfRange, MapperSeverity::kError,
                "皮肤条目引用的插槽下标越界：" + std::to_string(srcSlot.slotIndex), slotWhere);
        return result;
      }
      const SlotId slotId = skeleton.slots[static_cast<std::size_t>(srcSlot.slotIndex)].id;

      for (std::size_t attIndex = 0; attIndex < srcSlot.attachments.size(); ++attIndex) {
        const SourceAttachment& srcAtt = srcSlot.attachments[attIndex];
        const std::string where =
            slotWhere + "/attachments[" + std::to_string(attIndex) + "]";
        // 非 region 附件：B4 只遍历计数，这里同样不建对象（计数在 unsupported 里报告）。
        if (srcAtt.typeByte != static_cast<std::uint8_t>(AttachmentTypeByte::kRegion)) continue;

        const SourceRegionAttachment& srcRegion = srcAtt.region;
        if (!std::isfinite(srcRegion.x) || !std::isfinite(srcRegion.y) ||
            !std::isfinite(srcRegion.rotationDeg) || !std::isfinite(srcRegion.scaleX) ||
            !std::isfinite(srcRegion.scaleY) || !std::isfinite(srcRegion.width) ||
            !std::isfinite(srcRegion.height)) {
          addDiag(diags, MapperCode::kNonFiniteFloat, MapperSeverity::kError,
                  "region 附件出现非有限数值（NaN / ±Inf）", where);
          return result;
        }

        // 查找键 = path ?? name（name 为空时回退到皮肤键）。
        const std::string attachmentName = srcAtt.name.value_or(srcAtt.key);
        const std::string regionKey = srcRegion.path.value_or(attachmentName);

        AtlasRegionId regionId = regions.find(regionKey);
        if (regionId.isNone() && options.allowTrailingSpaceFallback) {
          const std::string trimmed = trimCopy(regionKey);
          if (trimmed != regionKey) {
            const AtlasRegionId degraded = regions.find(trimmed);
            if (!degraded.isNone()) {
              regionId = degraded;
              addDiag(diags, MapperCode::kAttachmentRegionDegraded, MapperSeverity::kWarning,
                      "附件名尾随空格，已按 trim 后的键降级匹配图集 region：'" + regionKey +
                          "' → '" + trimmed + "'（T-B3）",
                      where);
            }
          }
        }
        if (regionId.isNone()) {
          addDiag(diags, MapperCode::kAttachmentRegionUnresolved, MapperSeverity::kError,
                  "附件在图集里找不到对应 region（查找键 path ?? name）：'" + regionKey + "'",
                  where);
          return result;
        }

        SkinEntry entry;
        entry.id = ids.create(IdKind::SkinEntry);
        entry.slotId = slotId;
        entry.attachmentName = attachmentName;
        entry.attachment.regionId = regionId;
        entry.attachment.x = srcRegion.x;
        entry.attachment.y = srcRegion.y;
        entry.attachment.rotationDeg = srcRegion.rotationDeg;
        entry.attachment.scaleX = srcRegion.scaleX;
        entry.attachment.scaleY = srcRegion.scaleY;
        entry.attachment.width = srcRegion.width;
        entry.attachment.height = srcRegion.height;
        entry.attachment.color = colorFromRgba(srcRegion.colorRgba);
        skin.entries.push_back(std::move(entry));
        ++result.regionAttachmentCount;
      }
    }
    skeleton.skins.push_back(std::move(skin));
  }

  // ---- 不支持的能力：只计数 + 段名，不建半成品对象 ----
  reportUnsupported(diags, MapperCode::kUnsupportedCapability, "mesh 附件",
                    source.unsupported.mesh, "skins");
  reportUnsupported(diags, MapperCode::kUnsupportedCapability, "加权 mesh 附件",
                    source.unsupported.weightedMesh, "skins");
  reportUnsupported(diags, MapperCode::kUnsupportedCapability, "linked mesh 附件",
                    source.unsupported.linkedMesh, "skins");
  reportUnsupported(diags, MapperCode::kUnsupportedCapability, "加权 linked mesh 附件",
                    source.unsupported.weightedLinkedMesh, "skins");
  reportUnsupported(diags, MapperCode::kUnsupportedCapability, "boundingbox 附件",
                    source.unsupported.boundingBox, "skins");
  reportUnsupported(diags, MapperCode::kUnsupportedCapability, "未知类型附件",
                    source.unsupported.otherAttachmentType, "skins");
  reportUnsupported(diags, MapperCode::kUnsupportedCapability, "IK 约束",
                    source.unsupported.ikConstraints, "ik");
  reportUnsupported(diags, MapperCode::kUnsupportedCapability, "transform 约束",
                    source.unsupported.transformConstraints, "transform");
  reportUnsupported(diags, MapperCode::kUnsupportedCapability, "事件定义",
                    source.unsupported.events, "events");
  reportUnsupported(diags, MapperCode::kUnsupportedCapability, "动画",
                    source.unsupported.animations, "animations");
  reportUnsupported(diags, MapperCode::kUnsupportedCapability, "动画时间线",
                    source.unsupported.timelines, "animations");
  reportUnsupported(diags, MapperCode::kUnsupportedCapability, "deform 时间线",
                    source.unsupported.deformTimelines, "animations");

  bool hasError = false;
  for (const MapperDiagnostic& d : diags) {
    if (d.severity == MapperSeverity::kError) hasError = true;
  }
  result.valid = !hasError;
  if (result.valid) result.skeleton = std::move(skeleton);
  return result;
}

const char* toString(MapperCode code) noexcept {
  switch (code) {
    case MapperCode::kNone: return "NONE";
    case MapperCode::kUnsupportedVersion: return "UNSUPPORTED_VERSION";
    case MapperCode::kBoneParentOutOfRange: return "BONE_PARENT_OUT_OF_RANGE";
    case MapperCode::kSlotBoneOutOfRange: return "SLOT_BONE_OUT_OF_RANGE";
    case MapperCode::kSkinSlotOutOfRange: return "SKIN_SLOT_OUT_OF_RANGE";
    case MapperCode::kNonFiniteFloat: return "NON_FINITE_FLOAT";
    case MapperCode::kAttachmentRegionUnresolved: return "ATTACHMENT_REGION_UNRESOLVED";
    case MapperCode::kAttachmentRegionDegraded: return "ATTACHMENT_REGION_DEGRADED";
    case MapperCode::kUnsupportedCapability: return "UNSUPPORTED_CAPABILITY";
  }
  return "UNKNOWN";
}

}  // namespace loongbones::importers::spine32
