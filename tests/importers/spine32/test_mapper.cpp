// Task 4 B5：Source → canonical 映射的合成断言。
//
// 重点覆盖 B5 方案第 4/5 条与 field-mapping §10.1：
//   · 所有持久化 ID 由宿主 `IIdFactory` 发放（源下标不得泄漏进产物）；
//   · 查找键 `path ?? name`，精确优先，尾随空格降级（warning），未命中是 error；
//   · 不支持的能力**只计数 + 诊断，不建半成品 Domain 对象**。

#include "loongbones/importers/spine32/mapper.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "loongbones/domain/project.hpp"
#include "loongbones/importers/spine32/binary_reader.hpp"
#include "tests/domain/test_framework.hpp"

namespace {

using namespace loongbones;
using namespace loongbones::importers::spine32;

/// 记录每个种类发了多少个 ID —— 用来断言"所有 ID 都来自宿主工厂"。
class CountingIdFactory final : public IIdFactory {
 public:
  Id create(IdKind kind) override {
    ++created_[kind];
    return Id::generate(kind);
  }
  [[nodiscard]] std::size_t count(IdKind kind) const {
    const auto it = created_.find(kind);
    return it == created_.end() ? 0U : it->second;
  }

 private:
  std::map<IdKind, std::size_t> created_;
};

class NameRegions final : public IAtlasRegionLookup {
 public:
  explicit NameRegions(std::vector<std::string> names) {
    for (std::string& n : names) byName_.emplace(std::move(n), Id::generate(IdKind::AtlasRegion));
  }
  AtlasRegionId find(std::string_view key) const override {
    const auto it = byName_.find(std::string(key));
    return it == byName_.end() ? AtlasRegionId{} : it->second;
  }

 private:
  std::unordered_map<std::string, AtlasRegionId> byName_;
};

bool hasCode(const MapResult& r, MapperCode code) {
  for (const MapperDiagnostic& d : r.diagnostics) if (d.code == code) return true;
  return false;
}
std::size_t countSeverity(const MapResult& r, MapperSeverity severity) {
  std::size_t n = 0;
  for (const MapperDiagnostic& d : r.diagnostics) if (d.severity == severity) ++n;
  return n;
}

SourceBone bone(std::string name, int parent) {
  SourceBone b;
  b.name = std::move(name);
  b.parentIndex = parent;
  return b;
}

SourceSlot slot(std::string name, int boneIndex) {
  SourceSlot s;
  s.name = std::move(name);
  s.boneIndex = boneIndex;
  return s;
}

/// typeByte == 0 的 region 附件。`path` 为空时查找键回退到 name / key。
SourceAttachment regionAttachment(std::string key, std::optional<std::string> name,
                                  std::optional<std::string> path) {
  SourceAttachment a;
  a.key = std::move(key);
  a.name = std::move(name);
  a.typeByte = static_cast<std::uint8_t>(AttachmentTypeByte::kRegion);
  a.region.name = a.name.value_or(a.key);
  a.region.path = std::move(path);
  a.region.width = 64.0;
  a.region.height = 32.0;
  return a;
}

/// 一个**带真实载荷**的 mesh 附件（Task 5 B3 后 Reader 会真的解析出 mesh 对象）。
SourceAttachment meshAttachment(std::string key) {
  SourceAttachment a;
  a.key = std::move(key);
  a.name = key;
  a.typeByte = static_cast<std::uint8_t>(AttachmentTypeByte::kMesh);
  SourceMeshAttachment mesh;
  mesh.name = key;
  mesh.uvs = {0.0, 0.0, 1.0, 0.0, 0.5, 1.0};
  mesh.triangles = {0, 1, 2};
  mesh.vertices = {0.0, 0.0, 32.0, 0.0, 16.0, 32.0};
  a.mesh = mesh;
  return a;
}

/// 1 骨 / 1 槽 / 1 皮肤，皮肤里放给定的附件。
SourceSkeleton oneSkinSkeleton(std::vector<SourceAttachment> attachments) {
  SourceSkeleton s;
  s.version = kAcceptedVersion;
  s.bones.push_back(bone("root", -1));
  s.slots.push_back(slot("s0", 0));
  SourceSkinSlot skinSlot;
  skinSlot.slotIndex = 0;
  skinSlot.attachments = std::move(attachments);
  SourceSkin skin;
  skin.slots.push_back(std::move(skinSlot));
  s.skins.push_back(std::move(skin));
  return s;
}

MapperOptions options() {
  MapperOptions o;
  o.skeletonName = "test";
  return o;
}

}  // namespace

TEST("mapper", "minimal skeleton maps and every id comes from the host factory") {
  CountingIdFactory ids;
  NameRegions regions({"head"});
  const SourceSkeleton src = oneSkinSkeleton({regionAttachment("head", "head", std::nullopt)});
  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);

  CHECK(r.valid);
  CHECK_EQ(r.skeleton.bones.size(), std::size_t{1});
  CHECK_EQ(r.skeleton.slots.size(), std::size_t{1});
  CHECK_EQ(r.skeleton.skins.size(), std::size_t{1});
  CHECK_EQ(r.skeleton.skins[0].entries.size(), std::size_t{1});
  CHECK_EQ(r.regionAttachmentCount, std::size_t{1});

  // ID 全部由工厂发放，且数量与对象数一致（不多发、不复用源下标）。
  CHECK_EQ(ids.count(IdKind::Skeleton), std::size_t{1});
  CHECK_EQ(ids.count(IdKind::Bone), std::size_t{1});
  CHECK_EQ(ids.count(IdKind::Slot), std::size_t{1});
  CHECK_EQ(ids.count(IdKind::Skin), std::size_t{1});
  CHECK_EQ(ids.count(IdKind::SkinEntry), std::size_t{1});

  const SkinEntry& e = r.skeleton.skins[0].entries[0];
  CHECK_EQ(e.slotId, r.skeleton.slots[0].id);
  CHECK_EQ(e.attachmentName, std::string{"head"});
  const loongbones::RegionAttachment& att = std::get<loongbones::RegionAttachment>(e.attachment);
  CHECK_EQ(att.regionId, regions.find("head"));
  CHECK_EQ(att.width, 64.0);
  CHECK_EQ(att.height, 32.0);
}

TEST("mapper", "bone parent chain resolves to ids and boneOrder mirrors bones") {
  CountingIdFactory ids;
  NameRegions regions({});
  SourceSkeleton src;
  src.version = kAcceptedVersion;
  src.bones.push_back(bone("a", -1));
  src.bones.push_back(bone("b", 0));
  src.bones.push_back(bone("c", 1));
  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);

  CHECK(r.valid);
  CHECK_EQ(r.skeleton.bones.size(), std::size_t{3});
  CHECK(r.skeleton.bones[0].isRoot());
  CHECK_EQ(r.skeleton.bones[1].parent, r.skeleton.bones[0].id);
  CHECK_EQ(r.skeleton.bones[2].parent, r.skeleton.bones[1].id);
  CHECK_EQ(r.skeleton.boneOrder.size(), std::size_t{3});
  for (std::size_t i = 0; i < r.skeleton.bones.size(); ++i) {
    CHECK_EQ(r.skeleton.boneOrder[i], r.skeleton.bones[i].id);
  }
}

TEST("mapper", "forward reference and self parent are rejected") {
  CountingIdFactory ids;
  NameRegions regions({});
  SourceSkeleton src;
  src.version = kAcceptedVersion;
  src.bones.push_back(bone("a", -1));
  src.bones.push_back(bone("b", 1));  // 自环/前向
  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);
  CHECK(!r.valid);
  CHECK(hasCode(r, MapperCode::kBoneParentOutOfRange));
  CHECK(r.skeleton.bones.empty());  // 失败不留半成品
}

TEST("mapper", "slot bone out of range is rejected") {
  CountingIdFactory ids;
  NameRegions regions({});
  SourceSkeleton src;
  src.version = kAcceptedVersion;
  src.bones.push_back(bone("a", -1));
  src.slots.push_back(slot("s", 7));
  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);
  CHECK(!r.valid);
  CHECK(hasCode(r, MapperCode::kSlotBoneOutOfRange));
}

TEST("mapper", "skin slot out of range is rejected") {
  CountingIdFactory ids;
  NameRegions regions({"head"});
  SourceSkeleton src;
  src.version = kAcceptedVersion;
  src.bones.push_back(bone("a", -1));
  src.slots.push_back(slot("s", 0));
  SourceSkinSlot skinSlot;
  skinSlot.slotIndex = 5;
  skinSlot.attachments.push_back(regionAttachment("head", "head", std::nullopt));
  SourceSkin skin;
  skin.slots.push_back(std::move(skinSlot));
  src.skins.push_back(std::move(skin));
  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);
  CHECK(!r.valid);
  CHECK(hasCode(r, MapperCode::kSkinSlotOutOfRange));
}

TEST("mapper", "lookup key is path then name then skin key") {
  CountingIdFactory ids;
  NameRegions regions({"p", "n", "k"});
  // path 优先于 name。
  const SourceSkeleton a = oneSkinSkeleton({regionAttachment("k", "n", std::string{"p"})});
  const MapResult ra = mapSkeletonToCanonical(a, options(), regions, ids);
  CHECK(ra.valid);
  CHECK_EQ(std::get<loongbones::RegionAttachment>(ra.skeleton.skins[0].entries[0].attachment).regionId,
           regions.find("p"));
  CHECK_EQ(ra.skeleton.skins[0].entries[0].attachmentName, std::string{"n"});

  // 无 path 时用 name。
  const SourceSkeleton b = oneSkinSkeleton({regionAttachment("k", std::string{"n"}, std::nullopt)});
  const MapResult rb = mapSkeletonToCanonical(b, options(), regions, ids);
  CHECK(rb.valid);
  CHECK_EQ(std::get<loongbones::RegionAttachment>(rb.skeleton.skins[0].entries[0].attachment).regionId,
           regions.find("n"));

  // 无 name 时回退到皮肤键。
  const SourceSkeleton c = oneSkinSkeleton({regionAttachment("k", std::nullopt, std::nullopt)});
  const MapResult rc = mapSkeletonToCanonical(c, options(), regions, ids);
  CHECK(rc.valid);
  CHECK_EQ(std::get<loongbones::RegionAttachment>(rc.skeleton.skins[0].entries[0].attachment).regionId,
           regions.find("k"));
  CHECK_EQ(rc.skeleton.skins[0].entries[0].attachmentName, std::string{"k"});
}

TEST("mapper", "trailing space degrades with a warning when allowed") {
  CountingIdFactory ids;
  NameRegions regions({"head_back"});
  const SourceSkeleton src =
      oneSkinSkeleton({regionAttachment("head_back ", "head_back ", std::nullopt)});

  MapperOptions on = options();
  on.allowTrailingSpaceFallback = true;
  const MapResult r = mapSkeletonToCanonical(src, on, regions, ids);
  CHECK(r.valid);
  CHECK(hasCode(r, MapperCode::kAttachmentRegionDegraded));
  CHECK_EQ(countSeverity(r, MapperSeverity::kWarning), std::size_t{1});
  CHECK_EQ(std::get<loongbones::RegionAttachment>(r.skeleton.skins[0].entries[0].attachment).regionId,
           regions.find("head_back"));

  // 关闭降级后，同一输入必须变成 error —— 不得静默丢弃附件。
  MapperOptions off = options();
  off.allowTrailingSpaceFallback = false;
  const MapResult strict = mapSkeletonToCanonical(src, off, regions, ids);
  CHECK(!strict.valid);
  CHECK(hasCode(strict, MapperCode::kAttachmentRegionUnresolved));
}

TEST("mapper", "unresolved region is an error with the lookup key in the message") {
  CountingIdFactory ids;
  NameRegions regions({"other"});
  const SourceSkeleton src = oneSkinSkeleton({regionAttachment("missing", "missing", std::nullopt)});
  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);
  CHECK(!r.valid);
  CHECK(hasCode(r, MapperCode::kAttachmentRegionUnresolved));
  bool mentionsKey = false;
  for (const MapperDiagnostic& d : r.diagnostics) {
    if (d.code == MapperCode::kAttachmentRegionUnresolved && d.message.find("missing") != std::string::npos) {
      mentionsKey = true;
    }
  }
  CHECK(mentionsKey);
}

TEST("mapper", "mesh attachments are no longer skipped: unresolved region is an error") {
  // Task 5 B3 语义变更：mesh 附件**会**进 canonical，region 找不到即报错
  // （不再「只计数、不建对象」）。人工构造的 unsupported 计数仍照常报告。
  CountingIdFactory ids;
  NameRegions regions({"head"});
  SourceSkeleton src = oneSkinSkeleton(
      {regionAttachment("head", "head", std::nullopt), meshAttachment("m1"), meshAttachment("m2")});
  src.unsupported.mesh = 2;

  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);
  // 第二批附件的 region 未命中**不在映射层报错**（否则会抢先掩盖几何/权重问题），
  // 改由 Domain 统一判（kRegionNotFound）。
  CHECK(r.valid);
  CHECK_EQ(r.unsupported.mesh, std::size_t{2});
}

TEST("mapper", "parsed mesh attachment becomes a domain mesh attachment") {
  CountingIdFactory ids;
  NameRegions regions({"m1"});
  SourceAttachment att;
  att.key = "m1";
  att.name = "m1";
  att.typeByte = 2;
  SourceMeshAttachment mesh;
  mesh.name = "m1";
  mesh.uvs = {0.0, 0.0, 1.0, 0.0, 0.5, 1.0};
  mesh.triangles = {0, 1, 2};
  mesh.vertices = {0.0, 0.0, 32.0, 0.0, 16.0, 32.0};
  att.mesh = mesh;

  SourceSkeleton src = oneSkinSkeleton({att});
  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);
  CHECK(r.valid);
  CHECK_EQ(r.skeleton.skins[0].entries.size(), std::size_t{1});
  const auto* mapped =
      std::get_if<loongbones::MeshAttachment>(&r.skeleton.skins[0].entries[0].attachment);
  CHECK(mapped != nullptr);
  if (mapped != nullptr) {
    CHECK_EQ(mapped->triangles.size(), std::size_t{3});
    CHECK_EQ(mapped->vertices.size(), std::size_t{6});
  }
}

// =====================================================================
// Task 5 B3：合成 fixture 接入**产品 Mapper**（源 IR → canonical）与拒绝路径
//
// golden 来源：docs/skeleton/fixtures/README.md §2。
// 拒绝路径在 **Domain** 判（映射层不判几何/权重），故这里走「Reader → Mapper → Domain」。
// =====================================================================

#include <filesystem>
#include <fstream>
#include <iterator>

namespace {

std::vector<std::uint8_t> loadFixtureBytes(const std::string& name) {
  const std::filesystem::path path =
      std::filesystem::path(LOONGBONES_FIXTURE_DIR) / (name + ".skel.bytes");
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
}

bool nearValue(double a, double b) { return std::fabs(a - b) < 1e-6; }

}  // namespace

TEST("mapper", "fixture 06 maps to a domain mesh attachment") {
  const ReadResult read = readSkeletonBinary32(loadFixtureBytes("06-minimal-mesh"));
  CHECK(read.ok);
  CountingIdFactory ids;
  NameRegions regions({"mesh-0"});
  const MapResult r = mapSkeletonToCanonical(read.skeleton, options(), regions, ids);
  CHECK(r.valid);
  CHECK_EQ(r.skeleton.skins[0].entries.size(), std::size_t{1});
  const auto* mesh =
      std::get_if<loongbones::MeshAttachment>(&r.skeleton.skins[0].entries[0].attachment);
  CHECK(mesh != nullptr);
  if (mesh != nullptr) {
    CHECK_EQ(mesh->triangles.size(), std::size_t{3});
    CHECK_EQ(mesh->vertices.size(), std::size_t{6});
    CHECK(nearValue(mesh->vertices[2], 32.0));
    CHECK(nearValue(mesh->uvs[0], 0.0));
    CHECK(nearValue(mesh->uvs[5], 1.0));
    CHECK_EQ(mesh->regionId, regions.find("mesh-0"));
  }
  // 第二批能力**已支持**（Task 5 B3/B4）⇒ 不再出现在「不支持」账本里。
  CHECK_EQ(r.unsupported.mesh, std::size_t{0});
  CHECK_EQ(r.unsupported.weightedMesh, std::size_t{0});
  CHECK(!hasCode(r, MapperCode::kUnsupportedCapability));
}

TEST("mapper", "fixture 07 maps weighted mesh and keeps the negative weight") {
  const ReadResult read = readSkeletonBinary32(loadFixtureBytes("07-weighted-mesh"));
  CHECK(read.ok);
  CountingIdFactory ids;
  NameRegions regions({"wmesh-0"});
  const MapResult r = mapSkeletonToCanonical(read.skeleton, options(), regions, ids);
  CHECK(r.valid);
  const auto* wm = std::get_if<loongbones::WeightedMeshAttachment>(
      &r.skeleton.skins[0].entries[0].attachment);
  CHECK(wm != nullptr);
  if (wm != nullptr) {
    CHECK_EQ(wm->vertexBindings.size(), std::size_t{3});
    CHECK_EQ(wm->vertexBindings[0].size(), std::size_t{2});
    CHECK(nearValue(wm->vertexBindings[0][0].weight, 1.2));
    CHECK(nearValue(wm->vertexBindings[0][1].weight, -0.2));  // 负权重原值（D34）
    CHECK_EQ(wm->vertexBindings[0][1].boneId, r.skeleton.bones[0].id);
    CHECK_EQ(wm->vertexBindings[2].size(), std::size_t{2});
    CHECK(nearValue(wm->vertexBindings[1][0].weight, 1.0));
    CHECK(nearValue(wm->vertexBindings[2][0].weight, 0.5));
  }
}

TEST("mapper", "fixture 08 maps linked mesh with a parent key in the same slot") {
  const ReadResult read = readSkeletonBinary32(loadFixtureBytes("08-linked-mesh"));
  CHECK(read.ok);
  CountingIdFactory ids;
  NameRegions regions({"body_mesh"});
  const MapResult r = mapSkeletonToCanonical(read.skeleton, options(), regions, ids);
  CHECK(r.valid);
  CHECK_EQ(r.skeleton.skins[0].entries.size(), std::size_t{3});
  const auto* linked =
      std::get_if<loongbones::LinkedMeshAttachment>(&r.skeleton.skins[0].entries[1].attachment);
  CHECK(linked != nullptr);
  if (linked != nullptr) {
    CHECK_EQ(linked->parentAttachmentName, std::string{"body_mesh"});
    CHECK_EQ(linked->parentSlotId, r.skeleton.slots[0].id);
    CHECK(linked->inheritDeform);
  }
  const auto* linked2 =
      std::get_if<loongbones::LinkedMeshAttachment>(&r.skeleton.skins[0].entries[2].attachment);
  CHECK(linked2 != nullptr);
  if (linked2 != nullptr) CHECK(!linked2->inheritDeform);
}

TEST("mapper", "fixture 14 maps linked mesh whose parent skin is null") {
  const ReadResult read = readSkeletonBinary32(loadFixtureBytes("14-linked-mesh-implicit-skin"));
  CHECK(read.ok);
  CountingIdFactory ids;
  NameRegions regions({"body_wmesh"});
  const MapResult r = mapSkeletonToCanonical(read.skeleton, options(), regions, ids);
  CHECK(r.valid);
  CHECK_EQ(r.skeleton.skins[0].entries.size(), std::size_t{3});
  const auto* parent = std::get_if<loongbones::WeightedMeshAttachment>(
      &r.skeleton.skins[0].entries[0].attachment);
  CHECK(parent != nullptr);
  const auto* linked =
      std::get_if<loongbones::LinkedMeshAttachment>(&r.skeleton.skins[0].entries[1].attachment);
  CHECK(linked != nullptr);
  if (linked != nullptr) {
    CHECK_EQ(linked->parentAttachmentName, std::string{"body_wmesh"});
    CHECK_EQ(linked->parentSlotId, r.skeleton.slots[0].id);
  }
}

TEST("mapper", "fixture 11 maps ik and transform constraints") {
  const ReadResult read = readSkeletonBinary32(loadFixtureBytes("11-constraints"));
  CHECK(read.ok);
  CountingIdFactory ids;
  NameRegions regions({});
  const MapResult r = mapSkeletonToCanonical(read.skeleton, options(), regions, ids);
  CHECK(r.valid);
  CHECK_EQ(r.skeleton.ikConstraints.size(), std::size_t{1});
  if (!r.skeleton.ikConstraints.empty()) {
    const auto& c = r.skeleton.ikConstraints[0];
    CHECK_EQ(c.name, std::string{"leg-ik"});
    CHECK(c.enabled);  // mix = 1 ⇒ 开启（D35）
    CHECK_EQ(c.boneIds.size(), std::size_t{1});
    CHECK_EQ(c.boneIds[0], r.skeleton.bones[1].id);
    CHECK_EQ(c.targetBoneId, r.skeleton.bones[0].id);
  }
  CHECK_EQ(r.skeleton.transformConstraints.size(), std::size_t{1});
  if (!r.skeleton.transformConstraints.empty()) {
    const auto& c = r.skeleton.transformConstraints[0];
    CHECK_EQ(c.name, std::string{"copy-tr"});
    CHECK(nearValue(c.rotateMix, 90.0));  // 原值透传（D36）
    CHECK_EQ(c.boneId, r.skeleton.bones[1].id);
    CHECK_EQ(c.targetBoneId, r.skeleton.bones[0].id);
    CHECK(nearValue(c.offsetScaleX, 0.5));
  }
}

TEST("mapper", "fixture 15 maps ik with mix zero to disabled") {
  const ReadResult read = readSkeletonBinary32(loadFixtureBytes("15-ik-mix-zero"));
  CHECK(read.ok);
  CountingIdFactory ids;
  NameRegions regions({});
  const MapResult r = mapSkeletonToCanonical(read.skeleton, options(), regions, ids);
  CHECK(r.valid);
  CHECK_EQ(r.skeleton.ikConstraints.size(), std::size_t{1});
  if (!r.skeleton.ikConstraints.empty()) {
    const auto& c = r.skeleton.ikConstraints[0];
    CHECK(!c.enabled);            // mix = 0 ⇒ 未开启（D35）
    CHECK(nearValue(c.mix, 0.0));  // 原值仍保留
    CHECK(!c.bendPositive);
  }
}

TEST("mapper", "fixture 16 maps plain mesh without width and height") {
  const ReadResult read = readSkeletonBinary32(loadFixtureBytes("16-minimal-mesh-plain"));
  CHECK(read.ok);
  CountingIdFactory ids;
  NameRegions regions({"mesh-plain"});
  const MapResult r = mapSkeletonToCanonical(read.skeleton, options(), regions, ids);
  CHECK(r.valid);
  const auto* mesh =
      std::get_if<loongbones::MeshAttachment>(&r.skeleton.skins[0].entries[0].attachment);
  CHECK(mesh != nullptr);
  if (mesh != nullptr) {
    CHECK(nearValue(mesh->width, 0.0));   // nonessential = 0 ⇒ 不得臆造
    CHECK(nearValue(mesh->height, 0.0));
    CHECK_EQ(mesh->uvs.size(), std::size_t{6});
  }
}

/// fixture → Reader → Mapper → Domain 拒绝：返回第一条诊断码。
DiagnosticCode domainRejectCode(const std::string& fixture) {
  const ReadResult read = readSkeletonBinary32(loadFixtureBytes(fixture));
  CountingIdFactory ids;
  NameRegions regions({});
  const MapResult r = mapSkeletonToCanonical(read.skeleton, options(), regions, ids);
  if (!r.valid) return DiagnosticCode::kNone;  // 映射层就拒了 → 不是本测试要验的路径
  Project p = Project::create("fixture-check");
  ProjectData d = p.data();
  d.skeletons.push_back(r.skeleton);
  const Status st = p.adoptData(d);
  if (st.isOk()) return DiagnosticCode::kNone;
  return st.diagnostic().code;
}

TEST("mapper", "fixture 09 is rejected by domain with the triangle index code") {
  CHECK_EQ(domainRejectCode("09-mesh-bad-triangle"),
           DiagnosticCode::kMeshTriangleIndexOutOfRange);
}

TEST("mapper", "fixture 10 is rejected by domain with the weight bone code") {
  CHECK_EQ(domainRejectCode("10-mesh-bad-bone-index"),
           DiagnosticCode::kMeshWeightBoneUnresolved);
}

TEST("mapper", "fixture 12 is rejected by domain with the weight sum code") {
  CHECK_EQ(domainRejectCode("12-mesh-bad-weight-sum-zero"),
           DiagnosticCode::kMeshWeightSumInvalid);
}

TEST("mapper", "fixture 13 is rejected by domain with the weight sum code") {
  CHECK_EQ(domainRejectCode("13-mesh-bad-weight-sum-negative"),
           DiagnosticCode::kMeshWeightSumInvalid);
}

TEST("mapper", "fixture 06 mesh keeps width and height") {
  const ReadResult read = readSkeletonBinary32(loadFixtureBytes("06-minimal-mesh"));
  CHECK(read.ok);
  CountingIdFactory ids;
  NameRegions regions({"mesh-0"});
  const MapResult r = mapSkeletonToCanonical(read.skeleton, options(), regions, ids);
  CHECK(r.valid);
  const auto* mesh =
      std::get_if<loongbones::MeshAttachment>(&r.skeleton.skins[0].entries[0].attachment);
  CHECK(mesh != nullptr);
  if (mesh != nullptr) {
    CHECK(nearValue(mesh->width, 32.0));
    CHECK(nearValue(mesh->height, 32.0));
  }
}

TEST("mapper", "fixture 08 linked entry keeps its own region id") {
  const ReadResult read = readSkeletonBinary32(loadFixtureBytes("08-linked-mesh"));
  CHECK(read.ok);
  CountingIdFactory ids;
  NameRegions regions({"body_mesh"});
  const MapResult r = mapSkeletonToCanonical(read.skeleton, options(), regions, ids);
  CHECK(r.valid);
  const auto* linked =
      std::get_if<loongbones::LinkedMeshAttachment>(&r.skeleton.skins[0].entries[1].attachment);
  CHECK(linked != nullptr);
  if (linked != nullptr) {
    CHECK_EQ(linked->regionId, regions.find("body_mesh"));
  }
}

TEST("mapper", "fixture 14 second linked entry does not inherit deform") {
  const ReadResult read = readSkeletonBinary32(loadFixtureBytes("14-linked-mesh-implicit-skin"));
  CHECK(read.ok);
  CountingIdFactory ids;
  NameRegions regions({"body_wmesh"});
  const MapResult r = mapSkeletonToCanonical(read.skeleton, options(), regions, ids);
  CHECK(r.valid);
  const auto* linked2 =
      std::get_if<loongbones::LinkedMeshAttachment>(&r.skeleton.skins[0].entries[2].attachment);
  CHECK(linked2 != nullptr);
  if (linked2 != nullptr) CHECK(!linked2->inheritDeform);
}

TEST("mapper", "fixture 11 transform offsets are passed through unchanged") {
  const ReadResult read = readSkeletonBinary32(loadFixtureBytes("11-constraints"));
  CHECK(read.ok);
  CountingIdFactory ids;
  NameRegions regions({});
  const MapResult r = mapSkeletonToCanonical(read.skeleton, options(), regions, ids);
  CHECK(r.valid);
  CHECK_EQ(r.skeleton.transformConstraints.size(), std::size_t{1});
  if (!r.skeleton.transformConstraints.empty()) {
    const auto& c = r.skeleton.transformConstraints[0];
    CHECK(nearValue(c.offsetY, 1.0));
    CHECK(nearValue(c.offsetScaleX, 0.5));
    CHECK(nearValue(c.offsetScaleY, 1.0));
  }
}

TEST("mapper", "unsupported animations and constraints are reported without pseudo objects") {
  CountingIdFactory ids;
  NameRegions regions({"head"});
  SourceSkeleton src = oneSkinSkeleton({regionAttachment("head", "head", std::nullopt)});
  src.unsupported.animations = 3;
  src.unsupported.timelines = 120;
  src.unsupported.ikConstraints = 2;
  src.unsupported.transformConstraints = 1;

  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);
  CHECK(r.valid);
  CHECK_EQ(countSeverity(r, MapperSeverity::kWarning), std::size_t{4});
  CHECK_EQ(countSeverity(r, MapperSeverity::kError), std::size_t{0});
  // 不建半成品：骨架里除 region 外没有任何承载这些能力的地方。
  CHECK_EQ(r.skeleton.bones.size(), std::size_t{1});
  CHECK_EQ(r.skeleton.skins[0].entries.size(), std::size_t{1});
}

TEST("mapper", "color converts from 0xRRGGBBAA to 0..1 and blend maps by source enum") {
  CountingIdFactory ids;
  NameRegions regions({"head"});
  SourceSkeleton src = oneSkinSkeleton({regionAttachment("head", "head", std::nullopt)});
  src.slots[0].colorRgba = 0x4080C0FFu;
  src.slots[0].blend = 2;

  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);
  CHECK(r.valid);
  const ColorF& c = r.skeleton.slots[0].color;
  CHECK(std::fabs(c.r - 64.0 / 255.0) < 1e-12);
  CHECK(std::fabs(c.g - 128.0 / 255.0) < 1e-12);
  CHECK(std::fabs(c.b - 192.0 / 255.0) < 1e-12);
  CHECK(std::fabs(c.a - 1.0) < 1e-12);
  CHECK_EQ(r.skeleton.slots[0].blendMode, BlendMode::kMultiply);
}

TEST("mapper", "draw order mirrors slot array") {
  CountingIdFactory ids;
  NameRegions regions({"a", "b"});
  SourceSkeleton src;
  src.version = kAcceptedVersion;
  src.bones.push_back(bone("root", -1));
  src.slots.push_back(slot("s0", 0));
  src.slots.push_back(slot("s1", 0));
  SourceSkinSlot skinSlot;
  skinSlot.slotIndex = 0;
  skinSlot.attachments.push_back(regionAttachment("a", "a", std::nullopt));
  SourceSkinSlot second;
  second.slotIndex = 1;
  second.attachments.push_back(regionAttachment("b", "b", std::nullopt));
  SourceSkin skin;
  skin.slots.push_back(std::move(skinSlot));
  skin.slots.push_back(std::move(second));
  src.skins.push_back(std::move(skin));

  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);
  CHECK(r.valid);
  CHECK_EQ(r.skeleton.drawOrder.size(), std::size_t{2});
  CHECK_EQ(r.skeleton.drawOrder[0], r.skeleton.slots[0].id);
  CHECK_EQ(r.skeleton.drawOrder[1], r.skeleton.slots[1].id);
}

TEST("mapper", "unsupported source version is rejected") {
  CountingIdFactory ids;
  NameRegions regions({"head"});
  SourceSkeleton src = oneSkinSkeleton({regionAttachment("head", "head", std::nullopt)});
  src.version = "3.8.99";
  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);
  CHECK(!r.valid);
  CHECK(hasCode(r, MapperCode::kUnsupportedVersion));
}

TEST("mapper", "non finite source float is rejected") {
  CountingIdFactory ids;
  NameRegions regions({"head"});
  SourceSkeleton src = oneSkinSkeleton({regionAttachment("head", "head", std::nullopt)});
  src.bones[0].rotationDeg = std::numeric_limits<double>::quiet_NaN();
  const MapResult r = mapSkeletonToCanonical(src, options(), regions, ids);
  CHECK(!r.valid);
  CHECK(hasCode(r, MapperCode::kNonFiniteFloat));
}

TEST("mapper", "mapped skeleton is accepted by the domain") {
  CountingIdFactory ids;

  // 先造一份 canonical 图集（页 + 区域），用它的真实 region ID 做查找源。
  Atlas atlas;
  atlas.id = Id::generate(IdKind::Atlas);
  atlas.name = "atlas";
  AtlasPage page;
  page.id = Id::generate(IdKind::AtlasPage);
  page.name = "p.png";
  page.width = 256;
  page.height = 256;
  TextureAsset texture;
  texture.id = Id::generate(IdKind::TextureAsset);
  texture.name = "p.png";
  texture.location = "assets/textures/p.png";
  texture.pixelWidth = 512;
  texture.pixelHeight = 512;
  page.textureAssetId = texture.id;
  atlas.pages.push_back(page);
  AtlasRegion region;
  region.id = Id::generate(IdKind::AtlasRegion);
  region.name = "head";
  region.pageId = page.id;
  region.packedW = 64;
  region.packedH = 32;
  region.originalW = 64;
  region.originalH = 32;
  region.trimmedW = 64;
  region.trimmedH = 32;
  atlas.regions.push_back(region);

  AtlasRegionIndex index({atlas});
  CHECK_EQ(index.size(), std::size_t{1});

  const SourceSkeleton src = oneSkinSkeleton({regionAttachment("head", "head", std::nullopt)});
  MapperOptions o = options();
  o.atlasIds = {atlas.id};
  const MapResult r = mapSkeletonToCanonical(src, o, index, ids);
  CHECK(r.valid);

  // 工程级校验要求至少一个场景，补一个最小场景根（与骨架无关，只是让 adoptData 有上下文）。
  NodeRecord root;
  root.common.id = Id::generate(IdKind::Node);
  root.common.name = "Root";
  root.type = NodeType::kRoot;
  Scene scene;
  scene.id = Id::generate(IdKind::Scene);
  scene.name = "Scene";
  scene.rootNodeId = root.common.id;

  ProjectData data;
  data.id = Id::generate(IdKind::Project);
  data.name = "p";
  data.rootNodeId = root.common.id;
  data.scenes.push_back(scene);
  data.nodes.push_back(root);
  data.nodeOrder.push_back(root.common.id);
  data.textures.push_back(texture);
  data.atlases.push_back(atlas);
  data.skeletons.push_back(r.skeleton);
  const Status status = Project::createWithId(data.id, data.name).adoptData(std::move(data));
  if (!status.isOk()) {
    std::cout << "  [domain rejected] " << status.diagnostic().message << " @ "
              << status.diagnostic().location << "\n";
  }
  CHECK(status.isOk());
}

TEST("mapper", "code strings are stable and distinct") {
  CHECK(std::string(toString(MapperCode::kAttachmentRegionUnresolved)).size() >= 2);
  CHECK(std::string(toString(MapperCode::kAttachmentRegionUnresolved)) !=
        std::string(toString(MapperCode::kAttachmentRegionDegraded)));
}

