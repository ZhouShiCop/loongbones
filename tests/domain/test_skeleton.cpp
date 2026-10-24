// 骨架模型校验回归（Task 4 B2）
//
// 覆盖 Task4_Implementation_Plan.md B2 验收项：
//   · 新增 Domain 单测覆盖**每条拒绝规则**
//   · 合法最小骨架
//   · 随机 ID（不得由名称/下标/路径派生）
//   · 失败后接收对象快照不变（Back Task §4：完整深比较，不是只看返回状态）
//
// 本文件不测 Spine 二进制解析（B4）与 Atlas 文本映射（B5），只测 Domain 层。

#include "test_framework.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "loongbones/domain/project.hpp"
#include "loongbones/domain/skeleton.hpp"

using namespace loongbones;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// ---- 合法最小骨架的构件 ----

// 一条完整的 TextureAsset -> Atlas -> Page -> Region 链，返回 Region Id。
// 与 test_scene_atlas.cpp 的 buildRegion 同构，保证骨架附件能引用真实区域。
// index 用于在同一工程内建多条链：纹理/图集名称在工程内必须唯一。
AtlasRegionId buildRegionIn(Project& p, int index = 0, std::int32_t size = 32) {
  const std::string suffix = std::to_string(index);
  const TextureAssetId tex =
      p.createTextureAsset("t" + suffix, StorageMode::kExternalReference, "t" + suffix + ".png");
  const AtlasId at = p.createAtlas("a" + suffix);
  AtlasPage page;
  page.id = newAtlasPageId();
  page.textureAssetId = tex;
  page.width = size;
  page.height = size;
  CHECK(p.addAtlasPage(at, page).isOk());
  AtlasRegion r;
  r.id = newAtlasRegionId();
  r.pageId = page.id;
  r.packedW = size / 2;
  r.packedH = size / 2;
  r.rotation = RegionRotation::kNone;
  r.originalW = size / 2;
  r.originalH = size / 2;
  r.trimmedW = size / 2;
  r.trimmedH = size / 2;
  CHECK(p.addAtlasRegion(at, r).isOk());
  return r.id;
}

// 骨架：1 根骨 + 1 子骨 + 1 插槽 + default 皮肤（1 条目）。
// 附件引用传入的 region；atlasIds 指向传入的图集。
Skeleton makeSkeleton(const AtlasId& atlas, const AtlasRegionId& region) {
  Skeleton s;
  s.id = newSkeletonId();
  s.name = "hero";
  s.atlasIds.push_back(atlas);

  const BoneId root = newBoneId();
  const BoneId child = newBoneId();
  Bone b0;
  b0.id = root;
  b0.name = "root";
  Bone b1;
  b1.id = child;
  b1.name = "torso";
  b1.parent = root;
  b1.x = 12.5;
  b1.length = 40;
  s.bones = {b0, b1};
  s.boneOrder = {root, child};

  const SlotId slot = newSlotId();
  Slot sl;
  sl.id = slot;
  sl.name = "head";
  sl.boneId = child;
  sl.attachmentName = "head";
  s.slots = {sl};
  s.drawOrder = {slot};

  const SkinId skin = newSkinId();
  Skin sk;
  sk.id = skin;
  sk.name = "default";
  SkinEntry e;
  e.id = newSkinEntryId();
  e.slotId = slot;
  e.attachmentName = "head";
  RegionAttachment att;
  att.regionId = region;
  att.width = 32;
  att.height = 32;
  e.attachment = att;
  sk.entries = {e};
  s.skins = {sk};
  return s;
}

// 一份能通过 validate() 的 ProjectData，且带一个合法骨架。
ProjectData validProjectWithSkeleton() {
  Project p = Project::create("demo");
  const AtlasRegionId region = buildRegionIn(p);
  const AtlasId atlas = p.data().atlases[0].id;
  ProjectData d = p.data();
  d.skeletons.push_back(makeSkeleton(atlas, region));
  return d;
}

// ---- 骨架侧完整深比较（含所有新增字段）----

bool sameColor(const ColorF& a, const ColorF& b) { return a == b; }

bool sameAttachment(const Attachment& a, const Attachment& b) {
  if (a.index() != b.index()) return false;
  if (const auto* ra = std::get_if<RegionAttachment>(&a)) {
    const auto& rb = std::get<RegionAttachment>(b);
    return ra->regionId == rb.regionId && ra->x == rb.x && ra->y == rb.y &&
           ra->rotationDeg == rb.rotationDeg && ra->scaleX == rb.scaleX &&
           ra->scaleY == rb.scaleY && ra->width == rb.width && ra->height == rb.height &&
           sameColor(ra->color, rb.color);
  }
  // 第二批附件：字段逐个比较（mesh / 加权 / linked 各自有 operator==）。
  return a == b;
}

bool sameSkinEntry(const SkinEntry& a, const SkinEntry& b) {
  return a.id == b.id && a.slotId == b.slotId && a.attachmentName == b.attachmentName &&
         sameAttachment(a.attachment, b.attachment);
}

bool sameSkin(const Skin& a, const Skin& b) {
  if (!(a.id == b.id) || a.name != b.name) return false;
  if (a.entries.size() != b.entries.size()) return false;
  for (std::size_t i = 0; i < a.entries.size(); ++i) {
    if (!sameSkinEntry(a.entries[i], b.entries[i])) return false;
  }
  return true;
}

bool sameSlot(const Slot& a, const Slot& b) {
  return a.id == b.id && a.name == b.name && a.boneId == b.boneId && sameColor(a.color, b.color) &&
         a.blendMode == b.blendMode &&
         ((a.attachmentName.has_value() == b.attachmentName.has_value()) &&
          (!a.attachmentName.has_value() || *a.attachmentName == *b.attachmentName));
}

bool sameBone(const Bone& a, const Bone& b) {
  return a.id == b.id && a.name == b.name && a.parent == b.parent && a.x == b.x && a.y == b.y &&
         a.rotationDeg == b.rotationDeg && a.scaleX == b.scaleX && a.scaleY == b.scaleY &&
         a.shearXDeg == b.shearXDeg && a.shearYDeg == b.shearYDeg &&
         a.inheritRotation == b.inheritRotation && a.inheritScale == b.inheritScale &&
         a.length == b.length;
}

bool sameIdVector(const std::vector<Id>& a, const std::vector<Id>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (!(a[i] == b[i])) return false;
  }
  return true;
}

bool sameSourceRef(const SourceRef& a, const SourceRef& b) {
  if (a.formatId != b.formatId || a.formatVersion != b.formatVersion) return false;
  if (a.sourceHeaderHash.has_value() != b.sourceHeaderHash.has_value()) return false;
  if (a.sourceHeaderHash.has_value() && *a.sourceHeaderHash != *b.sourceHeaderHash) return false;
  if (a.contentHash != b.contentHash) return false;
  if (a.sourceUnit.has_value() != b.sourceUnit.has_value()) return false;
  if (a.sourceUnit.has_value() && *a.sourceUnit != *b.sourceUnit) return false;
  if (a.importedByPluginId != b.importedByPluginId) return false;
  return a.importedByPluginVersion == b.importedByPluginVersion;
}

bool sameSkeleton(const Skeleton& a, const Skeleton& b) {
  if (!(a.id == b.id) || a.name != b.name) return false;
  if (!sameIdVector(a.atlasIds, b.atlasIds)) return false;
  if (!sameIdVector(a.boneOrder, b.boneOrder)) return false;
  if (!sameIdVector(a.drawOrder, b.drawOrder)) return false;
  if (a.bones.size() != b.bones.size()) return false;
  for (std::size_t i = 0; i < a.bones.size(); ++i) {
    if (!sameBone(a.bones[i], b.bones[i])) return false;
  }
  if (a.slots.size() != b.slots.size()) return false;
  for (std::size_t i = 0; i < a.slots.size(); ++i) {
    if (!sameSlot(a.slots[i], b.slots[i])) return false;
  }
  if (a.skins.size() != b.skins.size()) return false;
  for (std::size_t i = 0; i < a.skins.size(); ++i) {
    if (!sameSkin(a.skins[i], b.skins[i])) return false;
  }
  if (a.sourceRef.has_value() != b.sourceRef.has_value()) return false;
  if (a.sourceRef.has_value() && !sameSourceRef(*a.sourceRef, *b.sourceRef)) return false;
  return true;
}

bool sameSkeletons(const std::vector<Skeleton>& a, const std::vector<Skeleton>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (!sameSkeleton(a[i], b[i])) return false;
  }
  return true;
}

/// 只比较骨架层的浅层集合信息（ID + 名称 + 规模），用于「接收对象未被污染」的粗判。
bool sameSkeletonShell(const std::vector<Skeleton>& a, const std::vector<Skeleton>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (!(a[i].id == b[i].id)) return false;
    if (a[i].name != b[i].name) return false;
    if (a[i].bones.size() != b[i].bones.size()) return false;
    if (a[i].slots.size() != b[i].slots.size()) return false;
    if (a[i].skins.size() != b[i].skins.size()) return false;
  }
  return true;
}

// 已持有合法骨架的接收方：用于验证「恶意候选被拒时，原数据（含骨架）不变」。
Project makeReceiverWithSkeleton() {
  Project p = Project::create("receiver");
  const AtlasRegionId region = buildRegionIn(p);
  const AtlasId atlas = p.data().atlases[0].id;
  ProjectData d = p.data();
  d.skeletons.push_back(makeSkeleton(atlas, region));
  CHECK(p.adoptData(d).isOk());  // 先确认真能装进去
  return p;
}

// Back Task §4：恶意数据必须被拒绝，且接收对象前后快照**完全**一致。
void expectAdoptRejectedKeepSkeleton(const ProjectData& malicious, DiagnosticCode expected) {
  Project p = makeReceiverWithSkeleton();
  const ProjectData before = p.data();
  CHECK(!p.adoptData(malicious).isOk());
  CHECK(p.lastStatus().code == expected);
  CHECK(sameSkeletons(before.skeletons, p.data().skeletons));
  // 骨架层之外的既有数据同样不得被动过
  CHECK(sameSkeletonShell(before.skeletons, p.data().skeletons));
}

bool containsCode(const std::vector<Diagnostic>& ds, DiagnosticCode code) {
  for (const Diagnostic& d : ds) {
    if (d.code == code) return true;
  }
  return false;
}

/// validateSkeleton() 应返回**全部**违规，而不是只报第一条：多错的候选要一次暴露，
/// 否则调用方只能靠反复试错逐个定位。
TEST(Skeleton, ValidateSkeletonReportsAllProblems) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  s.bones[1].x = kNaN;                                        // 数值非法
  s.slots[0].boneId = newBoneId();                            // 插槽悬空
  std::get<RegionAttachment>(s.skins[0].entries[0].attachment).regionId =
      newAtlasRegionId();  // 区域悬空
  s.atlasIds[0] = newAtlasId();                               // 图集悬空
  const std::vector<Diagnostic> ds = validateSkeleton(s, d);
  CHECK(containsCode(ds, DiagnosticCode::kSkeletonValueNotFinite));
  CHECK(containsCode(ds, DiagnosticCode::kSkeletonSlotBoneNotFound));
  CHECK(containsCode(ds, DiagnosticCode::kRegionNotFound));
  CHECK(containsCode(ds, DiagnosticCode::kSkeletonAtlasNotFound));
  // 至少报出上面 4 类，而不是遇到第一条就返回
  CHECK(ds.size() >= 4U);
}

}  // namespace

// ---- 合法最小骨架 ----

TEST(Skeleton, MinimalSkeletonAccepted) {
  Project p = Project::create("demo");
  const AtlasRegionId region = buildRegionIn(p);
  const AtlasId atlas = p.data().atlases[0].id;
  ProjectData d = p.data();
  d.skeletons.push_back(makeSkeleton(atlas, region));
  CHECK(p.adoptData(d).isOk());
  CHECK_EQ(p.data().skeletons.size(), std::size_t{1});
  CHECK(p.validate().empty());
}

// 无骨架的工程依然合法：skeletons 是新增集合，不因它为空而拒绝既有工程
// （v2 schema §1：project.skeletons 键恒存在，可为空数组）。
TEST(Skeleton, NoSkeletonStillValid) {
  Project p = Project::create("demo");
  buildRegionIn(p);
  CHECK(p.data().skeletons.empty());
  CHECK(p.validate().empty());
}

// 多骨架 + 每个骨架引用不同图集：合法。
TEST(Skeleton, MultipleSkeletonsAccepted) {
  Project p = Project::create("demo");
  const AtlasRegionId r1 = buildRegionIn(p, 0);
  const AtlasId a1 = p.data().atlases[0].id;
  const AtlasRegionId r2 = buildRegionIn(p, 1);
  const AtlasId a2 = p.data().atlases[1].id;
  ProjectData d = p.data();
  d.skeletons.push_back(makeSkeleton(a1, r1));
  d.skeletons.push_back(makeSkeleton(a2, r2));
  CHECK(p.adoptData(d).isOk());
  CHECK_EQ(p.data().skeletons.size(), std::size_t{2});
  CHECK(p.validate().empty());
}

// ---- 随机 ID ----

TEST(Skeleton, IdsAreRandomNotDerived) {
  Project p1 = Project::create("demo");
  const AtlasRegionId region = buildRegionIn(p1);
  const AtlasId atlas = p1.data().atlases[0].id;
  const Skeleton s1 = makeSkeleton(atlas, region);

  // 同名、同结构、同 region 的第二个骨架，ID 必须完全不同
  // （ID 不得由名称 / 数组下标 / 文件路径派生，ADR-0004）。
  const Skeleton s2 = makeSkeleton(atlas, region);
  CHECK(!(s1.id == s2.id));
  CHECK(!(s1.bones[0].id == s2.bones[0].id));
  CHECK(!(s1.slots[0].id == s2.slots[0].id));
  CHECK(!(s1.skins[0].id == s2.skins[0].id));
  CHECK(!(s1.skins[0].entries[0].id == s2.skins[0].entries[0].id));
  // 种类必须正确，否则强类型就失效了
  CHECK(s1.id.kind() == IdKind::Skeleton);
  CHECK(s1.bones[0].id.kind() == IdKind::Bone);
  CHECK(s1.slots[0].id.kind() == IdKind::Slot);
  CHECK(s1.skins[0].id.kind() == IdKind::Skin);
  CHECK(s1.skins[0].entries[0].id.kind() == IdKind::SkinEntry);
}

TEST(Skeleton, IdKindsAreDistinct) {
  // 同名不同种类的 ID 不得相等（强类型区分的意义所在）。
  const Id sk = newSkeletonId();
  const Id bone = newBoneId();
  CHECK(sk.kind() != bone.kind());
  CHECK(!(sk == bone));
  CHECK(sk.isWellFormed());
  CHECK(bone.isWellFormed());
}

// ---- ID 形态 / 种类 / 重复 ----

TEST(Skeleton, MalformedSkeletonIdRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].id = Id::fromString(IdKind::Skeleton, "not-a-ulid");
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kIdMalformed);
}

TEST(Skeleton, SkeletonIdKindMismatchRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].id = Id::generate(IdKind::Node);  // 种类错误
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kIdKindMismatch);
}

TEST(Skeleton, DuplicateSkeletonIdRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons.push_back(d.skeletons[0]);  // 整份复制，ID 相同
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kIdDuplicate);
}

TEST(Skeleton, BoneIdKindMismatchRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].bones[0].id = Id::generate(IdKind::Slot);
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kIdKindMismatch);
}

TEST(Skeleton, SlotIdKindMismatchRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].slots[0].id = Id::generate(IdKind::Bone);
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kIdKindMismatch);
}

TEST(Skeleton, SkinIdKindMismatchRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].skins[0].id = Id::generate(IdKind::Node);
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kIdKindMismatch);
}

TEST(Skeleton, SkinEntryIdKindMismatchRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].skins[0].entries[0].id = Id::generate(IdKind::Skin);
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kIdKindMismatch);
}

TEST(Skeleton, DuplicateBoneIdRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].bones[1].id = d.skeletons[0].bones[0].id;  // 复用骨骼 ID
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kIdDuplicate);
}

TEST(Skeleton, DuplicateSlotIdRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].slots.push_back(d.skeletons[0].slots[0]);
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kIdDuplicate);
}

TEST(Skeleton, DuplicateSkinIdRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].skins.push_back(d.skeletons[0].skins[0]);
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kIdDuplicate);
}

// ---- 骨骼层级 ----

TEST(Skeleton, EmptySkeletonRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].bones.clear();
  d.skeletons[0].boneOrder.clear();
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonEmpty);
}

TEST(Skeleton, NoRootBoneRejected) {
  ProjectData d = validProjectWithSkeleton();
  // 唯一根骨的 parent 指向子骨自身以外的另一个骨骼 -> 没有根骨
  d.skeletons[0].bones[0].parent = d.skeletons[0].bones[1].id;
  d.skeletons[0].bones[1].parent = d.skeletons[0].bones[1].id;  // 自环，同时无根
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonRootBoneMissing);
}

// 已裁定（2026-10-20）：根骨数量 **≥ 1 且允许多个**，不要求唯一。
// 这条测试的作用是钉住「多个」——否则日后容易被误当成疏漏，
// 对齐到 Scene 层的「根必须唯一」而改错。注意两者是**不同概念**：
//   · Scene 层 kRootNotUnique：场景树有且只有一个根节点（project.cpp）；
//   · Skeleton 层：可以有多个独立父链起点，Spine 语料确实存在。
TEST(Skeleton, MultipleRootBonesAccepted) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  // 追加第二根独立根骨（parent 省略），并挂一个子骨以示两条父链并存
  const BoneId root2 = newBoneId();
  const BoneId child2 = newBoneId();
  Bone b2;
  b2.id = root2;
  b2.name = "weapon";
  Bone b3;
  b3.id = child2;
  b3.name = "weapon-tip";
  b3.parent = root2;
  s.bones.push_back(b2);
  s.bones.push_back(b3);
  s.boneOrder.push_back(root2);
  s.boneOrder.push_back(child2);
  Project p = Project::create("demo");
  CHECK(p.adoptData(d).isOk());
  CHECK(p.validate().empty());
  // 仍不能因此推出「根骨唯一」：Scene 层的唯一根不受影响
  CHECK_EQ(p.data().scenes.size(), std::size_t{1});
}

// 多根骨时，根骨数量为 0 仍必须被拒（防止规则被误改成「无根也算合法」）
TEST(Skeleton, AllBonesParentedStillRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].bones[0].parent = d.skeletons[0].bones[1].id;  // 唯一根骨被挂到子骨下
  d.skeletons[0].bones[1].parent = d.skeletons[0].bones[0].id;  // 互指成环
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonRootBoneMissing);
}

TEST(Skeleton, BoneParentNotFoundRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].bones[1].parent = newBoneId();  // 指向不存在的骨骼
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonBoneParentNotFound);
}

TEST(Skeleton, BoneParentForwardRefRejected) {
  ProjectData d = validProjectWithSkeleton();
  // 子骨在前、根骨在后：parent 前向引用。此时仍有根骨（后者），
  // 但排序约束被破坏，必须拒绝。
  d.skeletons[0].bones[1].parent = d.skeletons[0].bones[0].id;
  std::swap(d.skeletons[0].bones[0], d.skeletons[0].bones[1]);
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonBoneParentForwardRef);
}

TEST(Skeleton, BoneOrderNotPermutationRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].boneOrder.pop_back();  // 漏引一个骨骼
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonBoneOrderNotPermutation);
}

TEST(Skeleton, BoneOrderDuplicateRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].boneOrder[1] = d.skeletons[0].boneOrder[0];  // 重复引用
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonBoneOrderNotPermutation);
}

TEST(Skeleton, BoneOrderExtraRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].boneOrder.push_back(newBoneId());  // 多引一个不存在的骨骼
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonBoneOrderNotPermutation);
}

// ---- 插槽与绘制顺序 ----

TEST(Skeleton, SlotBoneNotFoundRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].slots[0].boneId = newBoneId();  // 绑定不存在的骨骼
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonSlotBoneNotFound);
}

TEST(Skeleton, DrawOrderNotPermutationRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].drawOrder.clear();  // 漏引
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonDrawOrderNotPermutation);
}

TEST(Skeleton, DrawOrderDuplicateRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  // 两个**不同**的插槽（避免先触发插槽 ID 重复），但 drawOrder 里
  // 同一个插槽出现两次、另一个缺席：长度相同却不是排列。
  const SlotId slot2 = newSlotId();
  Slot extra = s.slots[0];
  extra.id = slot2;
  extra.name = "body";
  s.slots.push_back(extra);
  s.drawOrder = {s.slots[0].id, s.slots[0].id};
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonDrawOrderNotPermutation);
}

// ---- 皮肤 ----

TEST(Skeleton, SkinSlotNotFoundRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].skins[0].entries[0].slotId = newSlotId();  // 指向不存在的插槽
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonSkinSlotNotFound);
}

TEST(Skeleton, SkinEntryDuplicateNaturalKeyRejected) {
  ProjectData d = validProjectWithSkeleton();
  // 不同 ID、相同 (slotId, attachmentName)：自然键重复。
  // 这正是 SkinEntry 必须有稳定 ID 的原因——ID 不重复但语义键重复。
  SkinEntry dup = d.skeletons[0].skins[0].entries[0];
  dup.id = newSkinEntryId();
  d.skeletons[0].skins[0].entries.push_back(dup);
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonSkinEntryDuplicate);
}

TEST(Skeleton, SkinEntrySameNameDifferentSlotAccepted) {
  ProjectData d = validProjectWithSkeleton();
  // 同附件名挂到另一个插槽：自然键不同，合法。
  const SlotId slot2 = newSlotId();
  Slot extra = d.skeletons[0].slots[0];
  extra.id = slot2;
  extra.name = "body";
  d.skeletons[0].slots.push_back(extra);
  d.skeletons[0].drawOrder.push_back(slot2);
  SkinEntry e;
  e.id = newSkinEntryId();
  e.slotId = slot2;
  e.attachmentName = d.skeletons[0].skins[0].entries[0].attachmentName;
  e.attachment = d.skeletons[0].skins[0].entries[0].attachment;
  d.skeletons[0].skins[0].entries.push_back(e);
  Project p = Project::create("demo");
  CHECK(p.adoptData(d).isOk());
  CHECK(p.validate().empty());
}

TEST(Skeleton, MissingDefaultSkinRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].skins[0].name = "custom";
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonSkinMissingDefault);
}

TEST(Skeleton, ExtraNonDefaultSkinAccepted) {
  ProjectData d = validProjectWithSkeleton();
  Skin extra = d.skeletons[0].skins[0];
  extra.id = newSkinId();
  extra.name = "hat";
  d.skeletons[0].skins.push_back(extra);
  Project p = Project::create("demo");
  CHECK(p.adoptData(d).isOk());
  CHECK(p.validate().empty());
}

TEST(Skeleton, SkinOnlyNestedInSkeleton) {
  // Skin 不是 Project 顶层集合：ProjectData 没有顶层 skins 字段。
  // 这里以「骨架可整体删除且不留顶层痕迹」间接确认嵌套归属。
  ProjectData d = validProjectWithSkeleton();
  d.skeletons.clear();
  Project p = Project::create("demo");
  CHECK(p.adoptData(d).isOk());
  CHECK(p.validate().empty());
}

// ---- 跨集合引用闭合 ----

TEST(Skeleton, AttachmentRegionNotFoundRejected) {
  ProjectData d = validProjectWithSkeleton();
  std::get<RegionAttachment>(d.skeletons[0].skins[0].entries[0].attachment).regionId =
      newAtlasRegionId();
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kRegionNotFound);
}

TEST(Skeleton, AtlasIdNotFoundRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].atlasIds[0] = newAtlasId();  // 指向不存在的图集
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonAtlasNotFound);
}

TEST(Skeleton, DuplicateAtlasIdRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].atlasIds.push_back(d.skeletons[0].atlasIds[0]);
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kIdDuplicate);
}

TEST(Skeleton, EmptyAtlasIdsAccepted) {
  // 没有附件的骨架可以不引用任何图集（v2 schema §3.1：atlasIds 可选）。
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].atlasIds.clear();
  Project p = Project::create("demo");
  CHECK(p.adoptData(d).isOk());
  CHECK(p.validate().empty());
}

// ---- 非有限浮点 ----

TEST(Skeleton, NonFiniteBoneValueRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].bones[1].rotationDeg = kNaN;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonValueNotFinite);
}

TEST(Skeleton, InfiniteBoneScaleRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].bones[0].scaleX = kInf;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonValueNotFinite);
}

// =====================================================================
// Task 5 B1：第二批附件（mesh / 加权 / linked）与约束（IK / transform）
// 契约：docs/skeleton/mesh-and-constraint-rfc.md；裁定 D34 / D35 / D37 / D38
// =====================================================================

namespace {

AtlasRegionId regionOf(const Skeleton& s) {
  return std::get<RegionAttachment>(s.skins[0].entries[0].attachment).regionId;
}

bool hasCode(const std::vector<Diagnostic>& diags, DiagnosticCode code) {
  for (const Diagnostic& diag : diags) {
    if (diag.code == code) return true;
  }
  return false;
}

/// 3 顶点 / 1 三角 / uv 全在 0..1 的最小非加权 mesh。
MeshAttachment baseMesh(const Skeleton& s) {
  MeshAttachment m;
  m.regionId = regionOf(s);
  m.uvs = {0.0, 0.0, 1.0, 0.0, 0.5, 1.0};
  m.triangles = {0, 1, 2};
  m.vertices = {0.0, 0.0, 32.0, 0.0, 16.0, 32.0};
  m.width = 32.0;
  m.height = 32.0;
  return m;
}

/// 3 顶点 × 1 绑定，权重和均为 1（合法基线）。
WeightedMeshAttachment baseWeighted(const Skeleton& s) {
  WeightedMeshAttachment m;
  m.regionId = regionOf(s);
  m.uvs = {0.0, 0.0, 1.0, 0.0, 0.5, 1.0};
  m.triangles = {0, 1, 2};
  m.vertexBindings = {
      {VertexBinding{s.bones[1].id, 0.0, 0.0, 1.0}},
      {VertexBinding{s.bones[1].id, 32.0, 0.0, 1.0}},
      {VertexBinding{s.bones[0].id, 16.0, 32.0, 1.0}},
  };
  return m;
}

IkConstraint validIk(const Skeleton& s) {
  IkConstraint c;
  c.id = newIkConstraintId();
  c.name = "leg-ik";
  c.boneIds = {s.bones[1].id};
  c.targetBoneId = s.bones[0].id;
  c.mix = 1.0;
  c.enabled = true;  // mix != 0 ⇒ 开启（D35）
  c.bendPositive = true;
  return c;
}

}  // namespace

TEST(Skeleton, MeshAttachmentAccepted) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  MeshAttachment m = baseMesh(s);
  s.skins[0].entries[0].attachment = m;
  CHECK(validateSkeleton(s, d).empty());
}

TEST(Skeleton, MeshTriangleIndexOutOfRangeRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  MeshAttachment m = baseMesh(s);
  m.triangles = {0, 1, 9};  // 9 ≥ 顶点数 3
  s.skins[0].entries[0].attachment = m;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kMeshTriangleIndexOutOfRange);
}

TEST(Skeleton, MeshUvCountMismatchRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  MeshAttachment m = baseMesh(s);
  m.uvs = {0.0, 0.0, 1.0, 0.0};  // 4 ≠ 2×3
  s.skins[0].entries[0].attachment = m;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kMeshVertexGeometryInvalid);
}

TEST(Skeleton, MeshUvOutOfRangeRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  MeshAttachment m = baseMesh(s);
  m.uvs[5] = 1.5;  // 越出 [0,1]
  s.skins[0].entries[0].attachment = m;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kMeshUvOutOfRange);
}

TEST(Skeleton, WeightedMeshAccepted) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  WeightedMeshAttachment m = baseWeighted(s);
  s.skins[0].entries[0].attachment = m;
  CHECK(validateSkeleton(s, d).empty());
}

TEST(Skeleton, WeightedMeshNegativeWeightAcceptedWhenSumPositive) {
  // D34：单条负权重合法（语义＝该绑定对顶点无影响）；D37：判据是**权重和**。
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  WeightedMeshAttachment m = baseWeighted(s);
  m.vertexBindings[0] = {VertexBinding{s.bones[1].id, 0.0, 0.0, 1.2},
                          VertexBinding{s.bones[0].id, 10.0, 0.0, -0.2}};
  s.skins[0].entries[0].attachment = m;
  CHECK(validateSkeleton(s, d).empty());
}

TEST(Skeleton, WeightedMeshZeroWeightSumRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  WeightedMeshAttachment m = baseWeighted(s);
  m.vertexBindings[1] = {VertexBinding{s.bones[1].id, 32.0, 0.0, 0.0}};
  s.skins[0].entries[0].attachment = m;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kMeshWeightSumInvalid);
}

TEST(Skeleton, WeightedMeshNegativeWeightSumRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  WeightedMeshAttachment m = baseWeighted(s);
  m.vertexBindings[1] = {VertexBinding{s.bones[1].id, 32.0, 0.0, -0.5},
                          VertexBinding{s.bones[0].id, 0.0, 32.0, -0.5}};
  s.skins[0].entries[0].attachment = m;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kMeshWeightSumInvalid);
}

TEST(Skeleton, WeightedMeshEmptyBindingRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  WeightedMeshAttachment m = baseWeighted(s);
  m.vertexBindings[2] = {};
  s.skins[0].entries[0].attachment = m;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kMeshBindingCountInvalid);
}

TEST(Skeleton, WeightedMeshUnknownBoneRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  WeightedMeshAttachment m = baseWeighted(s);
  m.vertexBindings[0] = {VertexBinding{newBoneId(), 0.0, 0.0, 1.0}};
  s.skins[0].entries[0].attachment = m;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kMeshWeightBoneUnresolved);
}

TEST(Skeleton, LinkedMeshParentResolvedEvenWhenChildComesFirst) {
  // D38：父引用必须**两阶段**解析 —— 子条目排在父之前也必须解析成功。
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  SkinEntry parent = s.skins[0].entries[0];
  parent.attachmentName = "body";

  SkinEntry child;
  child.id = newSkinEntryId();
  child.slotId = s.slots[0].id;
  child.attachmentName = "body_linked";
  LinkedMeshAttachment lm;
  lm.regionId = regionOf(s);
  lm.parentSlotId = s.slots[0].id;
  lm.parentAttachmentName = "body";
  lm.inheritDeform = true;
  child.attachment = lm;

  s.skins[0].entries = {child, parent};  // **子在前**
  CHECK(validateSkeleton(s, d).empty());
}

TEST(Skeleton, LinkedMeshParentUnresolvedRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  LinkedMeshAttachment lm;
  lm.regionId = regionOf(s);
  lm.parentSlotId = s.slots[0].id;
  lm.parentAttachmentName = "no_such_attachment";
  s.skins[0].entries[0].attachment = lm;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kMeshLinkedParentUnresolved);
}

TEST(Skeleton, IkConstraintAccepted) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  s.ikConstraints = {validIk(s)};
  CHECK(validateSkeleton(s, d).empty());
}

TEST(Skeleton, IkConstraintEmptyChainRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  IkConstraint c = validIk(s);
  c.boneIds.clear();
  s.ikConstraints = {c};
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kConstraintChainEmpty);
}

TEST(Skeleton, IkConstraintUnresolvedTargetRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  IkConstraint c = validIk(s);
  c.targetBoneId = newBoneId();
  s.ikConstraints = {c};
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kConstraintTargetUnresolved);
}

TEST(Skeleton, IkConstraintWrongIdKindRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  IkConstraint c = validIk(s);
  c.id = newBoneId();  // 种类必须是 IkConstraint
  s.ikConstraints = {c};
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kIdKindMismatch);
}

TEST(Skeleton, TransformConstraintAcceptedWithOutOfRangeMix) {
  // D36：mix / offset **原值承载、不解释语义** —— 越出 [0,1] 也不得判非法。
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  TransformConstraint c;
  c.id = newTransformConstraintId();
  c.name = "copy";
  c.boneId = s.bones[1].id;
  c.targetBoneId = s.bones[0].id;
  c.rotateMix = -180.0;
  c.translateMix = 305.1;
  c.scaleMix = -316.4;
  s.transformConstraints = {c};
  CHECK(validateSkeleton(s, d).empty());
}

TEST(Skeleton, TransformConstraintUnresolvedBoneRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  TransformConstraint c;
  c.id = newTransformConstraintId();
  c.boneId = newBoneId();
  c.targetBoneId = s.bones[0].id;
  s.transformConstraints = {c};
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kConstraintTargetUnresolved);
}

TEST(Skeleton, NonFiniteWeightRejected) {
  ProjectData d = validProjectWithSkeleton();
  Skeleton& s = d.skeletons[0];
  WeightedMeshAttachment m = baseWeighted(s);
  m.vertexBindings[0] = {VertexBinding{s.bones[1].id, 0.0, 0.0, kNaN}};
  s.skins[0].entries[0].attachment = m;
  const std::vector<Diagnostic> diags = validateSkeleton(s, d);
  CHECK(hasCode(diags, DiagnosticCode::kSkeletonValueNotFinite));
  // 权重非有限时权重和**无法判定** ⇒ 不再叠加 MESH_WEIGHT_SUM_INVALID（避免重复报同一处）。
  CHECK(!hasCode(diags, DiagnosticCode::kMeshWeightSumInvalid));
}

TEST(Skeleton, NonFiniteAttachmentValueRejected) {
  ProjectData d = validProjectWithSkeleton();
  std::get<RegionAttachment>(d.skeletons[0].skins[0].entries[0].attachment).width = kNaN;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonValueNotFinite);
}

TEST(Skeleton, NonFiniteColorRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].slots[0].color.a = kInf;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonValueNotFinite);
}

// ---- SourceRef：整个对象可选（B0 退回补项）----

TEST(Skeleton, SourceRefOmittedIsNativeCreation) {
  // 原生创建的骨架没有来源信息，不得被拒绝，也不得产生诊断。
  ProjectData d = validProjectWithSkeleton();
  CHECK(!d.skeletons[0].sourceRef.has_value());
  Project p = Project::create("demo");
  CHECK(p.adoptData(d).isOk());
  CHECK(p.validate().empty());
}

TEST(Skeleton, SourceRefPresentAccepted) {
  ProjectData d = validProjectWithSkeleton();
  SourceRef r;
  r.formatId = "spine.binary";
  r.formatVersion = "3.2.01";
  r.contentHash = "ab";  // 形状校验属 Format 层（v2 schema §3.7）
  r.importedByPluginId = "loongbones.importers.spine32";
  r.importedByPluginVersion = 1;
  d.skeletons[0].sourceRef = r;
  Project p = Project::create("demo");
  CHECK(p.adoptData(d).isOk());
  CHECK(p.validate().empty());
  // 往返后字段不变（Domain 侧不做任何改写）
  const Skeleton& got = p.data().skeletons[0];
  CHECK(got.sourceRef.has_value());
  CHECK(got.sourceRef->formatId == "spine.binary");
  CHECK(got.sourceRef->importedByPluginVersion == 1);
}

TEST(Skeleton, SourceRefEmptyContentHashRejected) {
  ProjectData d = validProjectWithSkeleton();
  d.skeletons[0].sourceRef = SourceRef{};  // 全部必需字段为空
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonSourceRefInvalid);
}

TEST(Skeleton, SourceRefNegativePluginVersionRejected) {
  ProjectData d = validProjectWithSkeleton();
  SourceRef r;
  r.formatId = "spine.binary";
  r.formatVersion = "3.2.01";
  r.contentHash = "ab";
  r.importedByPluginId = "p";
  r.importedByPluginVersion = -1;
  d.skeletons[0].sourceRef = r;
  expectAdoptRejectedKeepSkeleton(d, DiagnosticCode::kSkeletonSourceRefInvalid);
}

TEST(Skeleton, SourceRefOptionalFieldsOmittedAccepted) {
  ProjectData d = validProjectWithSkeleton();
  SourceRef r;
  r.formatId = "spine.binary";
  r.formatVersion = "3.2.01";
  r.contentHash = "ab";
  r.importedByPluginId = "p";
  r.importedByPluginVersion = 0;
  d.skeletons[0].sourceRef = r;  // sourceHeaderHash / sourceUnit 均省略
  Project p = Project::create("demo");
  CHECK(p.adoptData(d).isOk());
  CHECK(p.validate().empty());
}

// ---- 诊断定位串 ----

TEST(Skeleton, DiagnosticLocationPointsAtOffender) {
  ProjectData d = validProjectWithSkeleton();
  const BoneId bad = newBoneId();
  d.skeletons[0].bones[1].parent = bad;
  Project p = makeReceiverWithSkeleton();
  CHECK(!p.adoptData(d).isOk());
  const Diagnostic& diag = p.lastStatus();
  CHECK(diag.code == DiagnosticCode::kSkeletonBoneParentNotFound);
  // 定位必须落在出错的骨骼上，而不是工程级空串
  CHECK(diag.location.find("/bones/") != std::string::npos);
  CHECK(diag.location.find(bad.str()) == std::string::npos);  // 是"引用者"而非"被引者"
  CHECK(diag.location.find(d.skeletons[0].bones[1].id.str()) != std::string::npos);
}

// ---- 既有 API / 语义不受影响 ----

TEST(Skeleton, ExistingApisUnchanged) {
  // Task 1 / Task 2 的既有 API 与语义不因新增 skeletons 集合而改变。
  Project p = Project::create("demo");
  const NodeId g = p.createGroupNode(p.rootNodeId(), GroupNodeData{"g", Transform(), true});
  CHECK(!g.isNone());
  const AtlasRegionId rid = buildRegionIn(p);
  CHECK(!rid.isNone());
  const NodeId sp =
      p.createSpriteNode(p.rootNodeId(), SpriteNodeData{"s", rid, Transform(), true});
  CHECK(!sp.isNone());
  CHECK(p.validate().empty());
  CHECK_EQ(p.nodesReferencingRegion(rid).size(), std::size_t{1});
  // 带骨架后这些语义依然成立
  const AtlasId atlas = p.data().atlases[0].id;
  ProjectData d = p.data();
  d.skeletons.push_back(makeSkeleton(atlas, rid));
  CHECK(p.adoptData(d).isOk());
  CHECK_EQ(p.nodesReferencingRegion(rid).size(), std::size_t{1});
  CHECK(p.validate().empty());
}
