// 骨架校验（Task 4 B2）。契约：docs/format/native-project-v2-skeleton.md §3/§5/§9
// 与 docs/skeleton/skeleton-domain-rfc.md §9。
//
// 结构上复用 atlas 的做法：所有判据集中在这里，写 API（本任务尚未提供）与
// Project::validate() / adoptData() 共用同一套函数，避免两处漂移。
//
// 校验不变量一览（与 v2 schema §9 的诊断等级表对应）：
//   error：ID 种类/重复、骨架为空、无根骨、父引用缺失/前向引用/成环、
//          boneOrder / drawOrder 不是排列、slot→bone 悬空、skin→slot 悬空、
//          entry 自然键重复、缺 default 皮肤、atlasIds 悬空、非有限浮点、
//          sourceRef 必需字段为空。
//   warning：模型尚未进入切片而产生的条目统计（v2 schema §10），本文件不涉及。

#include "loongbones/domain/skeleton.hpp"

#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "loongbones/domain/diagnostics.hpp"
#include "loongbones/domain/project.hpp"

namespace loongbones {
namespace {

// 诊断定位串，风格与 project.cpp 的 nodeLoc / atlasLoc 一致：
// "skeletons/<skeletonId>"，逐层下沉到 bones / slots / skins / entries。
std::string skeletonLoc(const SkeletonId& id) { return "skeletons/" + id.str(); }
std::string boneLoc(const SkeletonId& sk, const BoneId& id) {
  return skeletonLoc(sk) + "/bones/" + id.str();
}
std::string slotLoc(const SkeletonId& sk, const SlotId& id) {
  return skeletonLoc(sk) + "/slots/" + id.str();
}
std::string skinLoc(const SkeletonId& sk, const SkinId& id) {
  return skeletonLoc(sk) + "/skins/" + id.str();
}
std::string entryLoc(const SkeletonId& sk, const SkinId& skin, const SkinEntryId& id) {
  return skinLoc(sk, skin) + "/entries/" + id.str();
}

bool isFiniteNumber(double v) noexcept { return std::isfinite(v) != 0; }

/// ID 形态 / 种类 / 重复三态判定，与 project.cpp 的既有写法一一对应。
/// 用法：checkId(id, 期望种类, where, "骨骼", out)。
void checkId(const Id& id, IdKind kind, const std::string& where, const char* what,
             std::vector<Diagnostic>& out) {
  if (!id.isWellFormed()) {
    out.emplace_back(DiagnosticCode::kIdMalformed, std::string(what) + " ID 非法", where);
  } else if (id.kind() != kind) {
    out.emplace_back(DiagnosticCode::kIdKindMismatch, std::string(what) + " ID 种类错误", where);
  }
}

std::vector<Id> boneIdsOf(const std::vector<Bone>& bones) {
  std::vector<Id> out;
  out.reserve(bones.size());
  for (const Bone& b : bones) out.push_back(b.id);
  return out;
}

std::vector<Id> slotIdsOf(const std::vector<Slot>& slots) {
  std::vector<Id> out;
  out.reserve(slots.size());
  for (const Slot& sl : slots) out.push_back(sl.id);
  return out;
}

/// 数组是否恰好是另一集合的排列（不重不漏）。
bool isPermutationOf(const std::vector<Id>& order, const std::vector<Id>& members) {
  if (order.size() != members.size()) return false;
  std::unordered_set<std::string> seen;
  seen.reserve(order.size() * 2U + 1U);
  for (const Id& id : order) {
    if (id.isNone()) return false;  // 空串 ID 无法作为有效引用
    if (!seen.insert(id.str()).second) return false;  // 重复引用
  }
  if (seen.size() != members.size()) return false;
  for (const Id& id : members) {
    if (seen.find(id.str()) == seen.end()) return false;  // 漏引
  }
  return true;
}

/// 跨集合闭合：附件引用的 AtlasRegion 是否存在于工程任一图集中。
bool regionExists(const ProjectData& project, const AtlasRegionId& id) {
  for (const Atlas& a : project.atlases) {
    for (const AtlasRegion& r : a.regions) {
      if (r.id == id) return true;
    }
  }
  return false;
}

/// 跨集合闭合：skeleton.atlasIds 必须指向工程内存在的 Atlas。
void checkAtlasRefs(const Skeleton& s, const ProjectData& project, std::vector<Diagnostic>& out) {
  std::unordered_set<std::string> atlasIds;
  atlasIds.reserve(project.atlases.size() * 2U + 1U);
  for (const Atlas& a : project.atlases) atlasIds.insert(a.id.str());
  std::unordered_set<std::string> seen;
  seen.reserve(s.atlasIds.size() * 2U + 1U);
  for (const AtlasId& id : s.atlasIds) {
    if (!seen.insert(id.str()).second) {
      out.emplace_back(DiagnosticCode::kIdDuplicate, "骨架引用的图集 ID 重复",
                       skeletonLoc(s.id) + "/atlasIds");
      continue;
    }
    if (atlasIds.find(id.str()) == atlasIds.end()) {
      out.emplace_back(DiagnosticCode::kSkeletonAtlasNotFound, "骨架引用的图集不存在",
                       skeletonLoc(s.id) + "/atlasIds");
    }
  }
}

/// 骨骼侧：ID 形态/种类/重复、根骨存在性、parentId 闭合（缺失 / 前向 / 成环）。
void checkBones(const Skeleton& s, std::vector<Diagnostic>& out) {
  if (s.bones.empty()) {
    out.emplace_back(DiagnosticCode::kSkeletonEmpty, "骨架没有任何骨骼", skeletonLoc(s.id));
    return;  // 后续骨骼规则无从谈起，避免刷屏
  }

  std::unordered_map<std::string, std::size_t> indexById;  // ID 串 -> bones 下标
  indexById.reserve(s.bones.size() * 2U + 1U);
  for (std::size_t i = 0; i < s.bones.size(); ++i) {
    const Bone& b = s.bones[i];
    const std::string where = boneLoc(s.id, b.id);
    checkId(b.id, IdKind::Bone, where, "骨骼", out);
    if (b.id.isWellFormed() && !indexById.emplace(b.id.str(), i).second) {
      out.emplace_back(DiagnosticCode::kIdDuplicate, "骨骼 ID 重复", where);
    }
    if (!isFiniteNumber(b.x) || !isFiniteNumber(b.y) || !isFiniteNumber(b.rotationDeg) ||
        !isFiniteNumber(b.scaleX) || !isFiniteNumber(b.scaleY) ||
        !isFiniteNumber(b.shearXDeg) || !isFiniteNumber(b.shearYDeg) ||
        !isFiniteNumber(b.length)) {
      out.emplace_back(DiagnosticCode::kSkeletonValueNotFinite, "骨骼数值含 NaN 或 Inf", where);
    }
  }

  // 根骨骼：parentId 为空者。
  //
  // 契约（已裁定 2026-10-20，v2 schema §3.1）：根骨数量 **≥ 1 且允许多个**，
  // 不要求唯一。故只有 0 根才是错误。
  //
  // 不要与 Scene 层的「根必须唯一」（project.cpp 的 kRootNotUnique）对齐：
  // 那是场景树有且只有一个根节点，与骨架内多条独立父链起点是两回事，
  // Spine 语料中确实存在多根骨的骨架。
  std::size_t rootCount = 0;
  for (const Bone& b : s.bones) {
    if (b.isRoot()) ++rootCount;
  }
  if (rootCount == 0) {
    out.emplace_back(DiagnosticCode::kSkeletonRootBoneMissing, "骨架没有根骨骼",
                     skeletonLoc(s.id));
  }

  // parentId 闭合：缺失 / 前向引用 / 成环（RFC §9.1 三种全拒）。
  for (std::size_t i = 0; i < s.bones.size(); ++i) {
    const Bone& b = s.bones[i];
    if (b.isRoot()) continue;
    const auto it = indexById.find(b.parent.str());
    if (it == indexById.end()) {
      out.emplace_back(DiagnosticCode::kSkeletonBoneParentNotFound,
                       "骨骼父级指向不存在的骨骼", boneLoc(s.id, b.id));
      continue;
    }
    if (it->second >= i) {
      out.emplace_back(DiagnosticCode::kSkeletonBoneParentForwardRef,
                       "骨骼父级前向引用（须指向排序在前的骨骼）", boneLoc(s.id, b.id));
      continue;
    }
    // 前向引用已被排除，parent 下标必然递减，理论不可能成环。
    // 仍保留一次显式上溯作为纵深防御：若将来放宽顺序约束，这里仍能发现环。
    std::unordered_set<std::string> seen{b.parent.str()};
    std::size_t steps = 0;
    std::size_t cur = it->second;
    while (true) {
      if (++steps > s.bones.size()) {
        out.emplace_back(DiagnosticCode::kCycleDetected, "存在骨骼层级环", boneLoc(s.id, b.id));
        break;
      }
      const Bone& cb = s.bones[cur];
      if (cb.isRoot()) break;
      const auto next = indexById.find(cb.parent.str());
      if (next == indexById.end()) break;  // 由外层逐个 bones 的报告覆盖
      if (!seen.insert(cb.parent.str()).second) {
        out.emplace_back(DiagnosticCode::kCycleDetected, "存在骨骼层级环", boneLoc(s.id, b.id));
        break;
      }
      cur = next->second;
    }
  }

  if (!isPermutationOf(s.boneOrder, boneIdsOf(s.bones))) {
    out.emplace_back(DiagnosticCode::kSkeletonBoneOrderNotPermutation,
                     "boneOrder 与 bones[].id 不互为排列", skeletonLoc(s.id));
  }
}

/// 插槽侧：ID 形态/种类/重复、数值有限、boneId 闭合、drawOrder 排列。
void checkSlots(const Skeleton& s, std::vector<Diagnostic>& out) {
  std::unordered_set<std::string> boneIds;
  boneIds.reserve(s.bones.size() * 2U + 1U);
  for (const Bone& b : s.bones) boneIds.insert(b.id.str());

  std::unordered_set<std::string> slotIds;
  slotIds.reserve(s.slots.size() * 2U + 1U);
  for (const Slot& sl : s.slots) {
    const std::string where = slotLoc(s.id, sl.id);
    checkId(sl.id, IdKind::Slot, where, "插槽", out);
    if (sl.id.isWellFormed() && !slotIds.insert(sl.id.str()).second) {
      out.emplace_back(DiagnosticCode::kIdDuplicate, "插槽 ID 重复", where);
    }
    if (!sl.color.isFinite()) {
      out.emplace_back(DiagnosticCode::kSkeletonValueNotFinite, "插槽颜色含 NaN 或 Inf", where);
    }
    if (!isValidBlendMode(sl.blendMode)) {
      out.emplace_back(DiagnosticCode::kSkeletonValueNotFinite, "插槽混合模式取值非法", where);
    }
    if (boneIds.find(sl.boneId.str()) == boneIds.end()) {
      out.emplace_back(DiagnosticCode::kSkeletonSlotBoneNotFound, "插槽绑定的骨骼不存在", where);
    }
  }

  if (!isPermutationOf(s.drawOrder, slotIdsOf(s.slots))) {
    out.emplace_back(DiagnosticCode::kSkeletonDrawOrderNotPermutation,
                     "drawOrder 与 slots[].id 不互为排列", skeletonLoc(s.id));
  }
}

/// 皮肤侧：ID 形态/种类/重复、slotId 闭合、自然键 (slotId, attachmentName) 唯一、
/// 附件数值有限、附件 region 跨图集闭合。
void checkSkins(const Skeleton& s, const ProjectData& project, std::vector<Diagnostic>& out) {
  std::unordered_set<std::string> slotIds;
  slotIds.reserve(s.slots.size() * 2U + 1U);
  for (const Slot& sl : s.slots) slotIds.insert(sl.id.str());

  std::unordered_set<std::string> skinIds;
  skinIds.reserve(s.skins.size() * 2U + 1U);
  bool hasDefault = false;
  for (const Skin& skin : s.skins) {
    checkId(skin.id, IdKind::Skin, skinLoc(s.id, skin.id), "皮肤", out);
    if (skin.id.isWellFormed() && !skinIds.insert(skin.id.str()).second) {
      out.emplace_back(DiagnosticCode::kIdDuplicate, "皮肤 ID 重复", skinLoc(s.id, skin.id));
    }
    if (skin.name == "default") hasDefault = true;
  }
  if (!hasDefault) {
    out.emplace_back(DiagnosticCode::kSkeletonSkinMissingDefault, "骨架缺少 default 皮肤",
                     skeletonLoc(s.id));
  }

  for (const Skin& skin : s.skins) {
    // 自然键唯一：同一皮肤内 (slotId, attachmentName) 不得重复（RFC §9.1）。
    std::unordered_set<std::string> naturalKeys;
    naturalKeys.reserve(skin.entries.size() * 2U + 1U);
    for (const SkinEntry& e : skin.entries) {
      const std::string where = entryLoc(s.id, skin.id, e.id);
      checkId(e.id, IdKind::SkinEntry, where, "皮肤条目", out);
      if (slotIds.find(e.slotId.str()) == slotIds.end()) {
        out.emplace_back(DiagnosticCode::kSkeletonSkinSlotNotFound,
                         "皮肤条目指向不存在的插槽", where);
      }
      const std::string key = e.slotId.str() + "\x1f" + e.attachmentName;
      if (!naturalKeys.insert(key).second) {
        out.emplace_back(DiagnosticCode::kSkeletonSkinEntryDuplicate,
                         "同一皮肤内 (插槽, 附件名) 重复", where);
      }
      const RegionAttachment& a = e.attachment;
      if (!isFiniteNumber(a.x) || !isFiniteNumber(a.y) || !isFiniteNumber(a.rotationDeg) ||
          !isFiniteNumber(a.scaleX) || !isFiniteNumber(a.scaleY) ||
          !isFiniteNumber(a.width) || !isFiniteNumber(a.height) || !a.color.isFinite()) {
        out.emplace_back(DiagnosticCode::kSkeletonValueNotFinite, "附件数值含 NaN 或 Inf", where);
      }
      if (!regionExists(project, a.regionId)) {
        out.emplace_back(DiagnosticCode::kRegionNotFound, "附件引用的图集区域不存在", where);
      }
    }
  }
}

/// sourceRef 存在时的字段完整性。范围校验（hash 形状、color 0..1）属 Format 层
/// （v2 schema §3.3 / §3.7），Domain 只查必需字段非空。
void checkSourceRef(const Skeleton& s, std::vector<Diagnostic>& out) {
  if (!s.sourceRef.has_value()) return;  // 省略 = 合法（原生创建，B0 补项）
  const SourceRef& r = *s.sourceRef;
  const std::string where = skeletonLoc(s.id) + "/sourceRef";
  if (r.formatId.empty() || r.formatVersion.empty() || r.contentHash.empty() ||
      r.importedByPluginId.empty() || r.importedByPluginVersion < 0) {
    out.emplace_back(DiagnosticCode::kSkeletonSourceRefInvalid,
                     "sourceRef 必需字段为空或插件版本号为负", where);
  }
}

}  // namespace

bool ColorF::isFinite() const noexcept {
  return isFiniteNumber(r) && isFiniteNumber(g) && isFiniteNumber(b) && isFiniteNumber(a);
}

std::vector<Diagnostic> validateSkeleton(const Skeleton& s, const ProjectData& project) {
  std::vector<Diagnostic> out;
  // 骨架 ID 自身的形态 / 种类 / 跨骨架唯一性由 Project::validate() 统一处理。
  checkBones(s, out);
  checkSlots(s, out);
  checkSkins(s, project, out);
  checkAtlasRefs(s, project, out);
  checkSourceRef(s, out);
  return out;
}

}  // namespace loongbones
