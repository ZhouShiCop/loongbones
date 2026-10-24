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

  // 第二批附件的 region 查找：与 region 附件**同一套键与降级规则**（path ?? name）。
  //
  // reportUnresolved = false 时**不在映射层报错** —— 未命中会映射成"悬空 regionId"，
  // 由 Domain 统一判（kRegionNotFound）。这样第二批附件的几何 / 权重问题
  // （MESH_TRIANGLE_INDEX_OUT_OF_RANGE / MESH_WEIGHT_SUM_INVALID 等）
  // 不会被"图集里没这个 region"抢先掩盖（fixture golden 要求先报 MESH_* 码）。
  const auto resolveRegion = [&](const std::string& regionKey, const std::string& where,
                                 bool reportUnresolved) -> AtlasRegionId {
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
    if (regionId.isNone() && reportUnresolved) {
      addDiag(diags, MapperCode::kAttachmentRegionUnresolved, MapperSeverity::kError,
              "附件在图集里找不到对应 region（查找键 path ?? name）：'" + regionKey + "'", where);
    }
    return regionId;
  };

  // ---- 皮肤：region 附件 + 第二批附件（mesh / 加权 / linked）进入 canonical ----
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

      // linked mesh **没有自己的贴图**，region 来自**父附件** ⇒ 先在本 slot 内建立
      // 「附件名 → regionId」表，再映射 linked。父可能在另一个皮肤里，但 canonical 的
      // 父引用是 (slotId, attachmentName)，与皮肤无关（D38）。
      std::unordered_map<std::string, AtlasRegionId> parentRegionByName;
      for (const SourceAttachment& a : srcSlot.attachments) {
        if (a.linkedMesh.has_value()) continue;
        const std::string attName = a.name.value_or(a.key);
        std::string key = attName;
        if (a.mesh.has_value()) {
          key = a.mesh->path.value_or(attName);
        } else if (a.weightedMesh.has_value()) {
          key = a.weightedMesh->path.value_or(attName);
        } else {
          key = a.region.path.value_or(attName);
        }
        AtlasRegionId rid = regions.find(key);
        if (rid.isNone() && options.allowTrailingSpaceFallback) {
          const std::string trimmed = trimCopy(key);
          if (trimmed != key) {
            const AtlasRegionId degraded = regions.find(trimmed);
            if (!degraded.isNone()) {
              // 预扫只建表、**不产诊断**：降级 warning 由该附件自己的映射路径产出，
              // 否则同一附件会被报两次（既有断言 countSeverity == 1）。
              rid = degraded;
            }
          }
        }
        parentRegionByName.emplace(attName, rid);
      }

      for (std::size_t attIndex = 0; attIndex < srcSlot.attachments.size(); ++attIndex) {
        const SourceAttachment& srcAtt = srcSlot.attachments[attIndex];
        const std::string where =
            slotWhere + "/attachments[" + std::to_string(attIndex) + "]";
        // ---- 第二批附件（Task 5 B3）----
        // 分派依据是**实际解析出的对象**（Reader 保证 typeByte 与之一致），
        // 而不是 typeByte 本身 —— 人工构造的 IR 可能只有 typeByte 没有载荷。
        if (srcAtt.mesh.has_value() || srcAtt.weightedMesh.has_value() ||
            srcAtt.linkedMesh.has_value()) {
          const std::string attName = srcAtt.name.value_or(srcAtt.key);
          SkinEntry entry;
          entry.id = ids.create(IdKind::SkinEntry);
          entry.slotId = slotId;
          entry.attachmentName = attName;

          if (srcAtt.mesh.has_value()) {
            const SourceMeshAttachment& src = *srcAtt.mesh;
            const AtlasRegionId regionId = resolveRegion(src.path.value_or(attName), where, false);
            MeshAttachment out;
            out.regionId = regionId;
            out.uvs = src.uvs;
            for (std::uint16_t t : src.triangles) out.triangles.push_back(t);
            out.vertices = src.vertices;  // 槽骨骼局部坐标（N2）
            out.width = src.width;
            out.height = src.height;
            entry.attachment = out;
          } else if (srcAtt.weightedMesh.has_value()) {
            const SourceWeightedMeshAttachment& src = *srcAtt.weightedMesh;
            const AtlasRegionId regionId = resolveRegion(src.path.value_or(attName), where, false);
            WeightedMeshAttachment out;
            out.regionId = regionId;
            out.uvs = src.uvs;
            for (std::uint16_t t : src.triangles) out.triangles.push_back(t);
            out.width = src.width;
            out.height = src.height;
            for (const std::vector<SourceVertexBinding>& binds : src.vertexBindings) {
              std::vector<VertexBinding> mapped;
              mapped.reserve(binds.size());
              for (const SourceVertexBinding& b : binds) {
                VertexBinding vb;
                // 骨下标越界：**不**在映射层报错 —— 映射成"悬空绑定"，
                // 由 Domain 统一判 MESH_WEIGHT_BONE_UNRESOLVED（与 fixture golden 一致：
                // fixture 10 必须报该码，而不是映射层的下标越界）。
                vb.boneId = (b.boneIndex < 0 ||
                             static_cast<std::size_t>(b.boneIndex) >= boneIds.size())
                                ? BoneId{}
                                : boneIds[static_cast<std::size_t>(b.boneIndex)];
                vb.x = b.x;  // **该骨骼局部坐标**
                vb.y = b.y;
                vb.weight = b.weight;  // 源原值（D30；可为负，D34）
                mapped.push_back(vb);
              }
              out.vertexBindings.push_back(std::move(mapped));
            }
            entry.attachment = out;
          } else if (srcAtt.linkedMesh.has_value()) {
            const SourceLinkedMeshAttachment& src = *srcAtt.linkedMesh;
            // region 取**父附件**的（linked 自己没有贴图）；父找不到才回退到自身的键。
            AtlasRegionId regionId;
            const auto parentIt = parentRegionByName.find(src.parent);
            if (parentIt != parentRegionByName.end()) {
              regionId = parentIt->second;
            } else {
              regionId = resolveRegion(src.path.value_or(attName), where, false);
            }
            LinkedMeshAttachment out;
            out.regionId = regionId;
            // 父引用：canonical 用 (slotId, attachmentName)，**不复制几何**。
            // 源里父与子同 slot；父可能在**另一个皮肤**（parentSkin 显式指定，
            // 或 null = 同皮肤 —— D38 的两条路径），canonical 不记皮肤：
            // Domain 校验时在同骨架**所有皮肤**里解析该键，故两条路径都成立。
            out.parentSlotId = slotId;
            out.parentAttachmentName = src.parent;
            out.inheritDeform = src.inheritDeform;
            entry.attachment = out;
          } else {
            continue;  // 只有 typeByte、没有载荷（例如人工构造的 IR）→ 不建对象
          }
          skin.entries.push_back(std::move(entry));
          continue;
        }

        // 其它类型（boundingbox 等）不建对象（D32：只留类型占位）。
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
        RegionAttachment att;
        att.regionId = regionId;
        att.x = srcRegion.x;
        att.y = srcRegion.y;
        att.rotationDeg = srcRegion.rotationDeg;
        att.scaleX = srcRegion.scaleX;
        att.scaleY = srcRegion.scaleY;
        att.width = srcRegion.width;
        att.height = srcRegion.height;
        att.color = colorFromRgba(srcRegion.colorRgba);
        entry.attachment = att;
        skin.entries.push_back(std::move(entry));
        ++result.regionAttachmentCount;
      }
    }
    skeleton.skins.push_back(std::move(skin));
  }

  // ---- 骨架级约束（Task 5 B3）----
  for (const SourceIkConstraint& src : source.ikConstraints) {
    IkConstraint c;
    c.id = ids.create(IdKind::IkConstraint);
    c.name = src.name;
    for (int index : src.boneIndices) {
      if (index < 0 || static_cast<std::size_t>(index) >= boneIds.size()) {
        addDiag(diags, MapperCode::kSlotBoneOutOfRange, MapperSeverity::kError,
                "IK 约束引用的骨骼下标越界：" + std::to_string(index), "ikConstraints");
        return result;
      }
      c.boneIds.push_back(boneIds[static_cast<std::size_t>(index)]);
    }
    if (src.targetIndex < 0 || static_cast<std::size_t>(src.targetIndex) >= boneIds.size()) {
      addDiag(diags, MapperCode::kSlotBoneOutOfRange, MapperSeverity::kError,
              "IK 约束的目标骨下标越界：" + std::to_string(src.targetIndex), "ikConstraints");
      return result;
    }
    c.targetBoneId = boneIds[static_cast<std::size_t>(src.targetIndex)];
    c.mix = src.mix;              // 源原值（不解释语义，D36）
    c.enabled = (src.mix != 0.0);  // 是否开启（D35）
    c.bendPositive = src.bendPositive;
    skeleton.ikConstraints.push_back(std::move(c));
  }

  for (const SourceTransformConstraint& src : source.transformConstraints) {
    TransformConstraint c;
    c.id = ids.create(IdKind::TransformConstraint);
    c.name = src.name;
    if (src.boneIndex < 0 || static_cast<std::size_t>(src.boneIndex) >= boneIds.size()) {
      addDiag(diags, MapperCode::kSlotBoneOutOfRange, MapperSeverity::kError,
              "transform 约束引用的骨骼下标越界：" + std::to_string(src.boneIndex),
              "transformConstraints");
      return result;
    }
    c.boneId = boneIds[static_cast<std::size_t>(src.boneIndex)];
    if (src.targetIndex < 0 || static_cast<std::size_t>(src.targetIndex) >= boneIds.size()) {
      addDiag(diags, MapperCode::kSlotBoneOutOfRange, MapperSeverity::kError,
              "transform 约束的目标骨下标越界：" + std::to_string(src.targetIndex),
              "transformConstraints");
      return result;
    }
    c.targetBoneId = boneIds[static_cast<std::size_t>(src.targetIndex)];
    // 4 个 mix + 6 个 offset：**原值承载、不解释语义**（D36）
    c.rotateMix = src.rotateMix;
    c.translateMix = src.translateMix;
    c.scaleMix = src.scaleMix;
    c.shearMix = src.shearMix;
    c.offsetRotation = src.offsetRotation;
    c.offsetX = src.offsetX;
    c.offsetY = src.offsetY;
    c.offsetScaleX = src.offsetScaleX;
    c.offsetScaleY = src.offsetScaleY;
    c.offsetShearY = src.offsetShearY;
    skeleton.transformConstraints.push_back(std::move(c));
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
