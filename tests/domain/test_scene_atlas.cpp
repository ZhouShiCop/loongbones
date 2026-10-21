// 场景 / 根 / 图集 / 绘制顺序 / Sprite 回归（Task 2 + Task 2 Back Task）
//
// 覆盖：
//   2.1  Scene/root 关系由 validate() 完整闭合（单场景契约）
//   2.2  Atlas 元数据校验在写 API 与导入路径共用；含旋转 Region 的 trim/占位方向
//   2.3  SpriteNode -> AtlasRegion 公开创建/设置路径
//   3.   drawOrder() 含 Root；drawableOrder() 不含 Root
//   §4   每个导入失败用例都对「已持有有效数据」的接收对象做完整前后快照比较

#include "test_framework.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "loongbones/domain/project.hpp"

using namespace loongbones;

namespace {

NodeId addGroup(Project& p, const NodeId& parent, const char* name) {
  GroupNodeData d{name, Transform(), true};
  return p.createGroupNode(parent, d);
}

// 建一条完整合法的 TextureAsset -> Atlas -> AtlasPage 链，返回 Page Id。
AtlasPageId buildPage(Project& p, std::int32_t w, std::int32_t h) {
  const TextureAssetId tex = p.createTextureAsset("t", StorageMode::kExternalReference, "t.png");
  const AtlasId at = p.createAtlas("a");
  AtlasPage page;
  page.id = newAtlasPageId();
  page.textureAssetId = tex;
  page.width = w;
  page.height = h;
  p.addAtlasPage(at, page);
  return page.id;
}

// 建一条完整合法的 ... -> AtlasRegion 链，返回 Region Id（默认无裁切）。
AtlasRegionId buildRegion(Project& p, std::int32_t size = 32) {
  const AtlasPageId pageId = buildPage(p, size, size);
  const AtlasId at = p.data().atlases[0].id;
  AtlasRegion r;
  r.id = newAtlasRegionId();
  r.pageId = pageId;
  r.packedW = size / 2;
  r.packedH = size / 2;
  r.rotation = RegionRotation::kNone;
  r.originalW = size / 2;
  r.originalH = size / 2;
  r.trimmedW = size / 2;
  r.trimmedH = size / 2;
  p.addAtlasRegion(at, r);
  return r.id;
}

// 返回一份能通过 validate() 的合法 ProjectData（含一个图集）。
ProjectData validProjectWithAtlas() {
  Project p = Project::create("demo");
  addGroup(p, p.rootNodeId(), "g");
  buildRegion(p);
  return p.data();
}

// 构造一个已持有有效数据的接收对象：场景 + 根 + 分组 + 资源 + 图片节点 + 图集链。
// 断言失败导入不改接收方时，用它提供「非空且有内容」的前后快照。
Project makeReceiver() {
  Project p = Project::create("receiver");
  addGroup(p, p.rootNodeId(), "existing");
  const ImageAssetId a =
      p.createImageAsset("existing", StorageMode::kExternalReference, "e.png");
  p.createImageNode(p.rootNodeId(), ImageNodeData{"existingImg", a, Transform(), true});
  buildRegion(p);
  return p;
}

// ---- 完整深比较：ProjectData 的每一个字段都参与 ----

template <typename T>
bool sameOptId(const std::optional<T>& a, const std::optional<T>& b) {
  if (a.has_value() != b.has_value()) return false;
  return !a.has_value() || (*a == *b);
}

bool sameNode(const NodeRecord& a, const NodeRecord& b) {
  if (a.type != b.type) return false;
  if (!sameOptId(a.assetId, b.assetId)) return false;
  if (!sameOptId(a.regionId, b.regionId)) return false;
  if (!(a.common.id == b.common.id)) return false;
  if (a.common.name != b.common.name) return false;
  if (!(a.common.parent == b.common.parent)) return false;
  if (!(a.common.transform == b.common.transform)) return false;
  if (a.common.visible != b.common.visible) return false;
  if (a.common.locked != b.common.locked) return false;
  if (a.common.children.size() != b.common.children.size()) return false;
  for (std::size_t i = 0; i < a.common.children.size(); ++i) {
    if (!(a.common.children[i] == b.common.children[i])) return false;
  }
  return true;
}

bool sameAssetLike(const TextureAsset& a, const TextureAsset& b) {
  return a.id == b.id && a.name == b.name && a.storage == b.storage &&
         a.location == b.location && a.pixelWidth == b.pixelWidth &&
         a.pixelHeight == b.pixelHeight;
}

bool sameAssetLike(const ImageAsset& a, const ImageAsset& b) {
  return a.id == b.id && a.name == b.name && a.storage == b.storage &&
         a.location == b.location && a.pixelWidth == b.pixelWidth &&
         a.pixelHeight == b.pixelHeight;
}

bool sameScene(const Scene& a, const Scene& b) {
  return a.id == b.id && a.name == b.name && a.rootNodeId == b.rootNodeId;
}

bool sameProjectData(const ProjectData& a, const ProjectData& b) {
  if (!(a.id == b.id)) return false;
  if (a.name != b.name) return false;
  if (!(a.rootNodeId == b.rootNodeId)) return false;
  if (a.defaultSceneIndex != b.defaultSceneIndex) return false;

  if (a.scenes.size() != b.scenes.size()) return false;
  for (std::size_t i = 0; i < a.scenes.size(); ++i) {
    if (!sameScene(a.scenes[i], b.scenes[i])) return false;
  }

  if (a.nodes.size() != b.nodes.size()) return false;
  for (std::size_t i = 0; i < a.nodes.size(); ++i) {
    if (!sameNode(a.nodes[i], b.nodes[i])) return false;
  }

  if (a.nodeOrder.size() != b.nodeOrder.size()) return false;
  for (std::size_t i = 0; i < a.nodeOrder.size(); ++i) {
    if (!(a.nodeOrder[i] == b.nodeOrder[i])) return false;
  }

  if (a.assets.size() != b.assets.size()) return false;
  for (std::size_t i = 0; i < a.assets.size(); ++i) {
    if (!sameAssetLike(a.assets[i], b.assets[i])) return false;
  }

  if (a.textures.size() != b.textures.size()) return false;
  for (std::size_t i = 0; i < a.textures.size(); ++i) {
    if (!sameAssetLike(a.textures[i], b.textures[i])) return false;
  }

  if (a.atlases.size() != b.atlases.size()) return false;
  for (std::size_t i = 0; i < a.atlases.size(); ++i) {
    const Atlas& x = a.atlases[i];
    const Atlas& y = b.atlases[i];
    if (!(x.id == y.id)) return false;
    if (x.name != y.name) return false;

    if (x.pages.size() != y.pages.size()) return false;
    for (std::size_t j = 0; j < x.pages.size(); ++j) {
      const AtlasPage& p = x.pages[j];
      const AtlasPage& q = y.pages[j];
      if (!(p.id == q.id)) return false;
      if (p.name != q.name) return false;
      if (!(p.textureAssetId == q.textureAssetId)) return false;
      if (p.width != q.width || p.height != q.height) return false;
    }

    if (x.regions.size() != y.regions.size()) return false;
    for (std::size_t j = 0; j < x.regions.size(); ++j) {
      const AtlasRegion& p = x.regions[j];
      const AtlasRegion& q = y.regions[j];
      if (!(p.id == q.id)) return false;
      if (p.name != q.name) return false;
      if (!(p.pageId == q.pageId)) return false;
      if (p.packedX != q.packedX || p.packedY != q.packedY) return false;
      if (p.packedW != q.packedW || p.packedH != q.packedH) return false;
      if (p.rotation != q.rotation) return false;
      if (p.originalW != q.originalW || p.originalH != q.originalH) return false;
      if (p.trimmedW != q.trimmedW || p.trimmedH != q.trimmedH) return false;
      if (p.offsetX != q.offsetX || p.offsetY != q.offsetY) return false;
    }
  }
  return true;
}

// Back Task §4：恶意数据必须被拒绝，且接收对象前后快照**完全**一致
// （不是只看返回状态，也不是只抽查某一个字段）。
void expectAdoptRejected(const ProjectData& malicious, DiagnosticCode expected) {
  Project p = makeReceiver();  // 已持有有效数据
  const ProjectData before = p.data();
  CHECK(!p.adoptData(malicious).isOk());
  CHECK(p.lastStatus().code == expected);
  CHECK(sameProjectData(before, p.data()));
}

}  // namespace

// ---- 2.1 Scene / root 闭合 ----

TEST(Scene, SingleSceneContract) {
  Project p = Project::create("demo");
  CHECK_EQ(p.data().scenes.size(), std::size_t{1});
  CHECK_EQ(p.data().defaultSceneIndex, std::size_t{0});
  CHECK_EQ(p.data().scenes[0].rootNodeId, p.rootNodeId());
  CHECK(p.validate().empty());
}

TEST(Scene, MissingSceneRejected) {
  ProjectData d;
  d.id = Id::generate(IdKind::Project);
  expectAdoptRejected(d, DiagnosticCode::kSceneNotFound);
}

TEST(Scene, MultipleScenesRejected) {
  ProjectData d = validProjectWithAtlas();
  Scene s2;
  s2.id = Id::generate(IdKind::Scene);
  s2.rootNodeId = d.rootNodeId;
  d.scenes.push_back(s2);
  expectAdoptRejected(d, DiagnosticCode::kSceneCountMismatch);
}

TEST(Scene, SceneRootMissingNodeRejected) {
  ProjectData d = validProjectWithAtlas();
  d.scenes[0].rootNodeId = Id::generate(IdKind::Node);  // 指向不存在节点
  expectAdoptRejected(d, DiagnosticCode::kSceneRootInvalid);
}

TEST(Scene, SceneRootPointsToOrdinaryNodeRejected) {
  ProjectData d = validProjectWithAtlas();
  const NodeId gId = Id::generate(IdKind::Node);
  NodeRecord g;
  g.common.id = gId;
  g.common.parent = d.rootNodeId;
  g.type = NodeType::kGroup;
  d.nodes.push_back(g);
  d.nodeOrder.push_back(gId);
  d.scenes[0].rootNodeId = gId;
  d.rootNodeId = gId;
  expectAdoptRejected(d, DiagnosticCode::kSceneRootInvalid);
}

TEST(Scene, DefaultSceneRootMismatchRejected) {
  ProjectData d = validProjectWithAtlas();
  d.rootNodeId = Id::generate(IdKind::Node);  // 与场景根不一致
  expectAdoptRejected(d, DiagnosticCode::kSceneRootInvalid);
}

TEST(Scene, MultipleRootsRejected) {
  ProjectData d = validProjectWithAtlas();
  const NodeId r2 = Id::generate(IdKind::Node);
  NodeRecord r;
  r.common.id = r2;
  r.type = NodeType::kRoot;
  d.nodes.push_back(r);
  d.nodeOrder.push_back(r2);
  expectAdoptRejected(d, DiagnosticCode::kRootNotUnique);
}

TEST(Scene, RootHoldingRegionRejected) {
  ProjectData d = validProjectWithAtlas();
  // 场景根不得持有区域引用（校验规则 7 / 类型-数据自洽）
  const AtlasRegionId rid = d.atlases[0].regions[0].id;
  for (NodeRecord& n : d.nodes) {
    if (n.type == NodeType::kRoot) n.regionId = rid;
  }
  expectAdoptRejected(d, DiagnosticCode::kNodeTypeMismatch);
}

// ---- 2.2 Atlas 元数据校验（写 API 与导入共用）----

TEST(Atlas, ReferenceChainIsClosed) {
  Project p = Project::create("demo");
  const TextureAssetId tex = p.createTextureAsset("t", StorageMode::kExternalReference, "t.png");
  const AtlasId at = p.createAtlas("a");
  CHECK(!tex.isNone());
  CHECK(!at.isNone());

  AtlasPage page;
  page.id = newAtlasPageId();
  page.name = "p0";
  page.textureAssetId = tex;
  page.width = 256;
  page.height = 256;
  CHECK(p.addAtlasPage(at, page).isOk());
  CHECK(p.findPage(page.id) != nullptr);

  AtlasRegion r;
  r.id = newAtlasRegionId();
  r.name = "hero";
  r.pageId = page.id;
  r.packedX = 10;
  r.packedY = 20;
  r.packedW = 64;
  r.packedH = 48;
  r.rotation = RegionRotation::kNone;
  r.originalW = 64;
  r.originalH = 48;
  r.trimmedW = 64;
  r.trimmedH = 48;
  CHECK(p.addAtlasRegion(at, r).isOk());
  CHECK(p.findRegion(r.id) != nullptr);
  CHECK(r.hasNoTrim());
  CHECK(p.validate().empty());
}

// 非正方形旋转区域必须被判定为合法：
//   · 页内占位按旋转转置 -> (packedH, packedW)
//   · trim 方向与 original 一致 -> **不**转置
TEST(Atlas, RotatedRegionNonSquareAccepted) {
  Project p = Project::create("demo");
  const AtlasPageId pageId = buildPage(p, 100, 100);
  const AtlasId at = p.data().atlases[0].id;

  // 64(宽) x 48(高) 的精灵以 90° 打包：页内占位转置为 48 x 64，落在 100x100 内。
  // trim 不转置：trimmedW(64) <= originalW(64)、trimmedH(48) <= originalH(48)。
  // 若误按旋转转置 trim，本合法区域会被错判为非法。
  AtlasRegion r90;
  r90.id = newAtlasRegionId();
  r90.pageId = pageId;
  r90.packedW = 64;
  r90.packedH = 48;
  r90.rotation = RegionRotation::kDeg90;
  r90.originalW = 64;
  r90.originalH = 48;
  r90.trimmedW = 64;
  r90.trimmedH = 48;
  CHECK(p.addAtlasRegion(at, r90).isOk());

  AtlasRegion r270 = r90;
  r270.id = newAtlasRegionId();
  r270.rotation = RegionRotation::kDeg270;
  CHECK(p.addAtlasRegion(at, r270).isOk());

  CHECK(p.validate().empty());
}

// 90° 打包时页内占位必须按转置判定：
//   packedX=60、packedW=30、packedH=50 -> 占位 (packedH, packedW) = (50, 30)
//   60+50=110 > 100 -> 越界被拒；若忽略转置（用 packedW=30）会误判为合法。
TEST(Atlas, RotatedRegionTransposedOverflowRejected) {
  Project p = Project::create("demo");
  const AtlasPageId pageId = buildPage(p, 100, 100);
  const AtlasId at = p.data().atlases[0].id;

  AtlasRegion r;
  r.id = newAtlasRegionId();
  r.pageId = pageId;
  r.packedX = 60;
  r.packedY = 0;
  r.packedW = 30;
  r.packedH = 50;
  r.rotation = RegionRotation::kDeg90;
  r.originalW = 30;
  r.originalH = 50;
  r.trimmedW = 30;
  r.trimmedH = 50;
  CHECK(!p.addAtlasRegion(at, r).isOk());
  CHECK(p.lastStatus().code == DiagnosticCode::kInvalidRegionRect);
}

// trim 偏移按 original 方向判定（不受打包旋转影响）：
//   64x48 的非正方形区域，offsetY=5 -> 5+trimmedH(48) = 53 > originalH(48) -> 非法 trim。
TEST(Atlas, RotatedRegionTrimOverflowRejected) {
  Project p = Project::create("demo");
  const AtlasPageId pageId = buildPage(p, 100, 100);
  const AtlasId at = p.data().atlases[0].id;

  AtlasRegion r;
  r.id = newAtlasRegionId();
  r.pageId = pageId;
  r.packedW = 64;
  r.packedH = 48;
  r.rotation = RegionRotation::kDeg90;
  r.originalW = 64;
  r.originalH = 48;
  r.trimmedW = 64;
  r.trimmedH = 48;
  r.offsetY = 5;
  CHECK(!p.addAtlasRegion(at, r).isOk());
  CHECK(p.lastStatus().code == DiagnosticCode::kInvalidTrim);
}

TEST(Atlas, InvalidRegionRejected) {
  Project p = Project::create("demo");
  const TextureAssetId tex = p.createTextureAsset("t", StorageMode::kExternalReference, "t.png");
  const AtlasId at = p.createAtlas("a");
  AtlasPage page;
  page.id = newAtlasPageId();
  page.textureAssetId = tex;
  page.width = 100;
  page.height = 100;
  CHECK(p.addAtlasPage(at, page).isOk());
  AtlasRegion over;
  over.id = newAtlasRegionId();
  over.pageId = page.id;
  over.packedX = 90;
  over.packedY = 0;
  over.packedW = 64;
  over.packedH = 48;
  over.originalW = 64;
  over.originalH = 48;
  over.trimmedW = 64;
  over.trimmedH = 48;
  CHECK(!p.addAtlasRegion(at, over).isOk());
  CHECK(p.lastStatus().code == DiagnosticCode::kInvalidRegionRect);
  CHECK(p.validate().empty());
}

// 2.2 恶意 ProjectData 经 adoptData 被拒绝，且接收方（已持有有效数据）快照不变
TEST(Atlas, GlobalPageIdDuplicateRejected) {
  ProjectData d = validProjectWithAtlas();
  Atlas at2;
  at2.id = newAtlasId();
  at2.name = "a2";
  AtlasPage pg2;
  pg2.id = d.atlases[0].pages[0].id;  // 复用已有页 ID（跨 Atlas 重复）
  pg2.textureAssetId = d.textures[0].id;
  pg2.width = 10;
  pg2.height = 10;
  at2.pages.push_back(pg2);
  d.atlases.push_back(at2);
  expectAdoptRejected(d, DiagnosticCode::kIdDuplicate);
}

TEST(Atlas, CrossAtlasRegionIdDuplicateRejected) {
  ProjectData d = validProjectWithAtlas();
  Atlas at2;
  at2.id = newAtlasId();
  at2.name = "a2";
  AtlasPage pg2;
  pg2.id = newAtlasPageId();
  pg2.textureAssetId = d.textures[0].id;
  pg2.width = 10;
  pg2.height = 10;
  at2.pages.push_back(pg2);
  AtlasRegion r2;
  r2.id = d.atlases[0].regions[0].id;  // 复用已有区域 ID（跨 Atlas 重复）
  r2.pageId = pg2.id;
  r2.packedW = 5;
  r2.packedH = 5;
  r2.originalW = 5;
  r2.originalH = 5;
  r2.trimmedW = 5;
  r2.trimmedH = 5;
  at2.regions.push_back(r2);
  d.atlases.push_back(at2);
  expectAdoptRejected(d, DiagnosticCode::kIdDuplicate);
}

TEST(Atlas, RegionCrossAtlasPageRejected) {
  ProjectData d = validProjectWithAtlas();
  Atlas at2;
  at2.id = newAtlasId();
  at2.name = "a2";
  AtlasPage pg2;
  pg2.id = newAtlasPageId();
  pg2.textureAssetId = d.textures[0].id;
  pg2.width = 10;
  pg2.height = 10;
  at2.pages.push_back(pg2);
  AtlasRegion r2;
  r2.id = newAtlasRegionId();
  r2.pageId = d.atlases[0].pages[0].id;  // 引用另一图集的页
  r2.packedW = 5;
  r2.packedH = 5;
  r2.originalW = 5;
  r2.originalH = 5;
  r2.trimmedW = 5;
  r2.trimmedH = 5;
  at2.regions.push_back(r2);
  d.atlases.push_back(at2);
  expectAdoptRejected(d, DiagnosticCode::kPageNotInAtlas);
}

TEST(Atlas, OutOfBoundsRectRejected) {
  ProjectData d = validProjectWithAtlas();
  d.atlases[0].regions[0].packedW = 99999;
  d.atlases[0].regions[0].packedH = 99999;
  expectAdoptRejected(d, DiagnosticCode::kInvalidRegionRect);
}

TEST(Atlas, InvalidTrimRejected) {
  ProjectData d = validProjectWithAtlas();
  d.atlases[0].regions[0].offsetX = -5;
  expectAdoptRejected(d, DiagnosticCode::kInvalidTrim);
}

TEST(Atlas, InvalidRegionIdKindRejected) {
  ProjectData d = validProjectWithAtlas();
  d.atlases[0].regions[0].id = Id::generate(IdKind::Node);  // 错误种类
  expectAdoptRejected(d, DiagnosticCode::kIdKindMismatch);
}

TEST(Atlas, RegionInUseCannotBeDeleted) {
  Project p = Project::create("demo");
  const AtlasRegionId rid = buildRegion(p);
  const NodeId sp = p.createSpriteNode(p.rootNodeId(), SpriteNodeData{"s", rid, Transform(), true});
  CHECK(!sp.isNone());
  CHECK_EQ(p.nodesReferencingRegion(rid).size(), std::size_t{1});
  CHECK(!p.deleteAtlasRegion(rid).isOk());
  CHECK(p.lastStatus().code == DiagnosticCode::kRegionInUse);
}

// ---- 2.3 SpriteNode -> AtlasRegion 公开路径 ----

TEST(Sprite, ReferencesRegion) {
  Project p = Project::create("demo");
  const AtlasRegionId rid = buildRegion(p);
  const NodeId sp = p.createSpriteNode(p.rootNodeId(), SpriteNodeData{"s", rid, Transform(), true});
  CHECK(!sp.isNone());
  const NodeRecord* n = p.findNode(sp);
  CHECK(n != nullptr);
  CHECK(n->type == NodeType::kImage);
  CHECK(n->regionId == rid);
  CHECK(!n->assetId.has_value());  // 仅持区域引用
  CHECK_EQ(p.nodesReferencingRegion(rid).size(), std::size_t{1});
  CHECK(p.validate().empty());
}

TEST(Sprite, NonexistentRegionRejected) {
  Project p = Project::create("demo");
  const NodeId sp = p.createSpriteNode(
      p.rootNodeId(), SpriteNodeData{"s", Id::generate(IdKind::AtlasRegion), Transform(), true});
  CHECK(sp.isNone());
  CHECK(p.lastStatus().code == DiagnosticCode::kRegionNotFound);
}

TEST(Sprite, SetNodeRegionOnNonImageRejected) {
  Project p = Project::create("demo");
  const NodeId g = addGroup(p, p.rootNodeId(), "g");
  const AtlasRegionId rid = buildRegion(p);
  CHECK(!p.setNodeRegion(g, rid).isOk());
  CHECK(p.lastStatus().code == DiagnosticCode::kNodeTypeMismatch);
}

TEST(Sprite, SetNodeRegionClearsAsset) {
  Project p = Project::create("demo");
  const ImageAssetId a =
      p.createImageAsset("img", StorageMode::kExternalReference, "img.png");
  const NodeId img = p.createImageNode(p.rootNodeId(), ImageNodeData{"img", a, Transform(), true});
  const AtlasRegionId rid = buildRegion(p);
  CHECK(p.setNodeRegion(img, rid).isOk());
  const NodeRecord* n = p.findNode(img);
  CHECK(!n->assetId.has_value());  // 切换到区域引用后清除资产引用
  CHECK(n->regionId == rid);
  CHECK(p.validate().empty());
}

TEST(Sprite, MultipleSpritesShareRegionIndependentTransforms) {
  Project p = Project::create("demo");
  const AtlasRegionId rid = buildRegion(p);
  Transform t1;
  t1.x = 5;
  t1.y = 7;
  Transform t2;
  t2.x = -3;
  t2.y = 9;
  const NodeId s1 = p.createSpriteNode(p.rootNodeId(), SpriteNodeData{"s1", rid, t1, true});
  const NodeId s2 = p.createSpriteNode(p.rootNodeId(), SpriteNodeData{"s2", rid, t2, true});
  CHECK(p.findNode(s1)->common.transform.x == 5.0);
  CHECK(p.findNode(s1)->common.transform.y == 7.0);
  CHECK(p.findNode(s2)->common.transform.x == -3.0);
  CHECK(p.findNode(s2)->common.transform.y == 9.0);
  CHECK_EQ(p.nodesReferencingRegion(rid).size(), std::size_t{2});
  CHECK(p.validate().empty());
}

// ---- 3. 绘制顺序语义 ----

TEST(Order, DrawOrderIsSelfThenChildren) {
  Project p = Project::create("demo");
  const NodeId a = addGroup(p, p.rootNodeId(), "a");
  const NodeId b = addGroup(p, p.rootNodeId(), "b");
  const NodeId a1 = addGroup(p, a, "a1");
  const NodeId a2 = addGroup(p, a, "a2");

  const std::vector<const NodeRecord*> o = p.drawOrder();
  CHECK_EQ(o.size(), std::size_t{5});
  CHECK_EQ(o[0]->common.id, p.rootNodeId());
  CHECK_EQ(o[1]->common.id, a);
  CHECK_EQ(o[2]->common.id, a1);
  CHECK_EQ(o[3]->common.id, a2);
  CHECK_EQ(o[4]->common.id, b);
  const std::vector<const NodeRecord*> again = p.drawOrder();
  CHECK_EQ(o.size(), again.size());
  for (std::size_t i = 0; i < o.size(); ++i) {
    CHECK_EQ(o[i]->common.id, again[i]->common.id);
  }
  CHECK(p.validate().empty());
}

TEST(Order, DrawableOrderExcludesRoot) {
  Project p = Project::create("demo");
  addGroup(p, p.rootNodeId(), "a");
  addGroup(p, p.rootNodeId(), "b");
  const std::vector<const NodeRecord*> draw = p.drawOrder();
  const std::vector<const NodeRecord*> render = p.drawableOrder();
  CHECK_EQ(draw.size(), std::size_t{3});
  CHECK_EQ(render.size(), std::size_t{2});  // 不含 Root
  for (const NodeRecord* n : render) CHECK(n->type != NodeType::kRoot);
}
