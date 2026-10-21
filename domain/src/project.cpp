// Project 实现。核心纪律（Task1 5）:
//   每个写操作都是「先完整校验, 再一次性提交」, 失败时不留半完成状态。
//   错误用 Status 返回, 不用异常。

#include "loongbones/domain/project.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace loongbones {
namespace {

std::string nodeLoc(const NodeId& id) { return "nodes/" + id.str(); }
std::string assetLoc(const ImageAssetId& id) { return "assets/" + id.str(); }
std::string sceneLoc(const SceneId& id) { return "scenes/" + id.str(); }
std::string textureLoc(const TextureAssetId& id) { return "textures/" + id.str(); }
std::string atlasLoc(const AtlasId& id) { return "atlases/" + id.str(); }
std::string pageLoc(const AtlasPageId& id) { return "atlasPages/" + id.str(); }
std::string regionLoc(const AtlasRegionId& id) { return "atlasRegions/" + id.str(); }

/// 区域在页内实际占位（考虑 90/270 旋转转置），用 int64 防止有符号整数溢出。
bool regionPackFitsPage(const AtlasRegion& r, const AtlasPage& p) {
  const bool swap = rotationSwapsExtent(r.rotation);
  const std::int64_t occW = swap ? r.packedH : r.packedW;
  const std::int64_t occH = swap ? r.packedW : r.packedH;
  const std::int64_t px = r.packedX, py = r.packedY;
  const std::int64_t pw = p.width, ph = p.height;
  if (px < 0 || py < 0 || occW <= 0 || occH <= 0) return false;
  return px + occW <= pw && py + occH <= ph;
}

/// trim 偏移叠加裁切尺寸须落在原始尺寸内，用 int64 防溢出。
///
/// 方向约定（与 atlas.hpp 的 AtlasRegion 注释一致）：
///   trim 是在「原始图像」上做的裁切，因此 trimmedW/H 与 offsetX/Y 的方向
///   与 originalW/H 一致，**不做转置**。打包旋转只影响页内实际占位
///   （见 regionPackFitsPage），不影响 trim 与原始尺寸的比对。
bool regionTrimFits(const AtlasRegion& r) {
  if (r.offsetX < 0 || r.offsetY < 0) return false;
  const std::int64_t ox = r.offsetX, oy = r.offsetY;
  const std::int64_t tw = r.trimmedW, th = r.trimmedH;
  const std::int64_t ow = r.originalW, oh = r.originalH;
  if (tw <= 0 || th <= 0) return false;
  return ox + tw <= ow && oy + th <= oh;
}

/// 页元数据 + 纹理引用闭合（纹理存在性跨全工程校验）。供写 API 与 validate 共用。
Status checkPage(const ProjectData& d, const AtlasPage& page) {
  if (!page.id.isWellFormed())
    return Status::fail(DiagnosticCode::kIdMalformed, "页 ID 格式非法", pageLoc(page.id));
  if (page.id.kind() != IdKind::AtlasPage)
    return Status::fail(DiagnosticCode::kIdKindMismatch, "页 ID 种类错误", pageLoc(page.id));
  if (page.width <= 0 || page.height <= 0)
    return Status::fail(DiagnosticCode::kInvalidRegionRect, "页尺寸非正", pageLoc(page.id));
  bool texOk = false;
  for (const TextureAsset& t : d.textures) {
    if (t.id == page.textureAssetId) { texOk = true; break; }
  }
  if (!texOk)
    return Status::fail(DiagnosticCode::kTextureAssetNotFound, "页引用的纹理资产不存在", pageLoc(page.id));
  return Status::ok();
}

/// 区域元数据 + 页引用闭合（页必须属于同一图集）+ trim 边界。供写 API 与 validate 共用。
Status checkRegion(const Atlas& atlas, const AtlasRegion& r) {
  if (!r.id.isWellFormed())
    return Status::fail(DiagnosticCode::kIdMalformed, "区域 ID 格式非法", regionLoc(r.id));
  if (r.id.kind() != IdKind::AtlasRegion)
    return Status::fail(DiagnosticCode::kIdKindMismatch, "区域 ID 种类错误", regionLoc(r.id));
  if (!isValidRotation(r.rotation))
    return Status::fail(DiagnosticCode::kInvalidRotation, "旋转值非法", regionLoc(r.id));
  if (!r.hasValidRect() || !r.hasValidSizes())
    return Status::fail(DiagnosticCode::kInvalidRegionRect, "矩形或尺寸非正", regionLoc(r.id));
  if (!regionTrimFits(r))
    return Status::fail(DiagnosticCode::kInvalidTrim, "trim 偏移/尺寸非法", regionLoc(r.id));
  const AtlasPage* page = nullptr;
  for (const AtlasPage& p : atlas.pages) {
    if (p.id == r.pageId) { page = &p; break; }
  }
  if (page == nullptr)
    return Status::fail(DiagnosticCode::kPageNotInAtlas, "区域引用的页不属于本图集", regionLoc(r.id));
  if (!regionPackFitsPage(r, *page))
    return Status::fail(DiagnosticCode::kInvalidRegionRect, "矩形越出页边界", regionLoc(r.id));
  return Status::ok();
}

}
  // namespace
// ---- 内部工具 ----

std::size_t Project::indexOfNode(const NodeId& id) const {
  for (std::size_t i = 0; i < data_.nodeOrder.size(); ++i) {
    if (data_.nodeOrder[i] == id) return i;
  }
  return kNpos;
}

std::size_t Project::indexOfAsset(const ImageAssetId& id) const {
  for (std::size_t i = 0; i < data_.assets.size(); ++i) {
    if (data_.assets[i].id == id) return i;
  }
  return kNpos;
}

std::size_t Project::indexOfAtlas(const AtlasId& id) const {
  for (std::size_t i = 0; i < data_.atlases.size(); ++i) {
    if (data_.atlases[i].id == id) return i;
  }
  return kNpos;
}
void Project::detachChild(NodeCommon& parent, const NodeId& child) {
  auto it = std::find(parent.children.begin(), parent.children.end(), child);
  if (it != parent.children.end()) parent.children.erase(it);
}

Status Project::recordFailure(DiagnosticCode code, std::string message, std::string location) {
  lastStatus_ = Diagnostic(code, std::move(message), std::move(location));
  return Status::fail(lastStatus_.code, lastStatus_.message, lastStatus_.location);
}
// ---- 构造 ----

Project Project::create(std::string projectName) {
  Project p;
  p.data_.id = Id::generate(IdKind::Project);
  p.data_.name = std::move(projectName);

  // 场景根：唯一、无父、恒等局部变换、不持有图片内容。
  // 类型用 kRoot 明确身份，避免"任意 GroupNode 都能变成根"。
  NodeRecord root;
  root.common.id = Id::generate(IdKind::Node);
  root.common.name = "root";
  root.common.transform = Transform{};  // 恒等
  root.type = NodeType::kRoot;
  root.assetId = std::nullopt;

  Scene scene;
  scene.id = Id::generate(IdKind::Scene);
  scene.name = "default";
  scene.rootNodeId = root.common.id;

  p.data_.nodes.push_back(root);
  p.data_.nodeOrder.push_back(root.common.id);
  p.data_.scenes.push_back(scene);
  p.data_.defaultSceneIndex = 0;
  p.data_.rootNodeId = root.common.id;  // Task 1 兼容字段
  return p;
}

Project Project::createWithId(ProjectId id, std::string projectName) {
  Project p;
  p.data_.id = std::move(id);
  p.data_.name = std::move(projectName);
  return p;
}
// ---- 查询 ----

const NodeRecord* Project::findNode(const NodeId& id) const {
  const std::size_t i = indexOfNode(id);
  return i == kNpos ? nullptr : &data_.nodes[i];
}

const ImageAsset* Project::findAsset(const ImageAssetId& id) const {
  const std::size_t i = indexOfAsset(id);
  return i == kNpos ? nullptr : &data_.assets[i];
}

const NodeRecord* Project::parentOf(const NodeId& id) const {
  const NodeRecord* n = findNode(id);
  if (n == nullptr || n->common.parent.isNone()) return nullptr;
  return findNode(n->common.parent);
}
std::vector<const NodeRecord*> Project::childrenOf(const NodeId& id) const {
  std::vector<const NodeRecord*> out;
  const NodeRecord* n = findNode(id);
  if (n == nullptr) return out;
  out.reserve(n->common.children.size());
  for (const NodeId& c : n->common.children) {
    const NodeRecord* cn = findNode(c);
    if (cn != nullptr) out.push_back(cn);
  }
  return out;
}

bool Project::isDescendantOf(const NodeId& node, const NodeId& ancestor) const {
  if (node == ancestor) return true;
  const NodeRecord* n = findNode(node);
  std::size_t guard = 0;
  while (n != nullptr && guard++ < data_.nodes.size() + 1) {
    if (n->common.parent == ancestor) return true;
    n = findNode(n->common.parent);
  }
  return false;
}
std::vector<const NodeRecord*> traverseDepthFirst(const ProjectData& data) {
  std::vector<const NodeRecord*> out;
  const NodeRecord* root = nullptr;
  for (const NodeRecord& n : data.nodes) {
    if (n.common.id == data.rootNodeId) { root = &n; break; }
  }
  if (root == nullptr) return out;

  // 迭代式 DFS, 避免深树递归爆栈; visited 兜底环（环由 validate 报告）
  std::unordered_set<std::string> visited;
  std::vector<const NodeRecord*> stack{root};
  while (!stack.empty()) {
    const NodeRecord* cur = stack.back();
    stack.pop_back();
    if (!visited.insert(cur->common.id.str()).second) continue;
    out.push_back(cur);    for (auto it = cur->common.children.rbegin(); it != cur->common.children.rend(); ++it) {
      for (const NodeRecord& n : data.nodes) {
        if (n.common.id == *it) { stack.push_back(&n); break; }
      }
    }
  }
  return out;
}

std::vector<const NodeRecord*> Project::nodesInOrder() const { return traverseDepthFirst(data_); }

std::vector<const NodeRecord*> Project::nodesReferencing(const ImageAssetId& id) const {
  std::vector<const NodeRecord*> out;
  for (const NodeRecord* n : traverseDepthFirst(data_)) {
    if (n->type == NodeType::kImage && n->assetId.has_value() && *n->assetId == id) {
      out.push_back(n);
    }
  }
  return out;
}
// ---- 资源 ----

ImageAssetId Project::createImageAsset(std::string assetName) {
  return createImageAsset(std::move(assetName), StorageMode::kExternalReference, std::string());
}

ImageAssetId Project::createImageAsset(std::string assetName, StorageMode storage,
                                       std::string location) {
  for (const ImageAsset& a : data_.assets) {
    if (a.name == assetName) {
      recordFailure(DiagnosticCode::kAssetNameDuplicate, "资源名已被占用", assetLoc(a.id));
      return ImageAssetId();
    }
  }
  ImageAsset a;
  a.id = Id::generate(IdKind::ImageAsset);
  a.name = std::move(assetName);
  a.storage = storage;
  a.location = std::move(location);
  data_.assets.push_back(std::move(a));
  succeed();
  return data_.assets.back().id;
}
Status Project::deleteImageAsset(const ImageAssetId& id) {
  const std::size_t ai = indexOfAsset(id);
  if (ai == kNpos) {
    return recordFailure(DiagnosticCode::kAssetNotFound, "资源不存在", assetLoc(id));
  }
  const std::vector<const NodeRecord*> refs = nodesReferencing(id);
  if (!refs.empty()) {
    return recordFailure(DiagnosticCode::kAssetInUse,
                         "资源仍被 " + std::to_string(refs.size()) + " 个节点引用",
                         assetLoc(id));
  }
  data_.assets.erase(data_.assets.begin() + static_cast<std::ptrdiff_t>(ai));
  return succeed();
}

// ---- 图集资源层（Task 2）----

TextureAssetId Project::createTextureAsset(std::string name, StorageMode storage,
                                           std::string location) {
  for (const TextureAsset& t : data_.textures) {
    if (t.name == name) {
      recordFailure(DiagnosticCode::kAssetNameDuplicate, "纹理资产重名", "textures");
      return TextureAssetId();
    }
  }
  TextureAsset t;
  t.id = newTextureAssetId();
  t.name = std::move(name);
  t.storage = storage;
  t.location = std::move(location);
  const TextureAssetId id = t.id;
  data_.textures.push_back(std::move(t));
  succeed();
  return id;
}

AtlasId Project::createAtlas(std::string name) {
  for (const Atlas& a : data_.atlases) {
    if (a.name == name) {
      recordFailure(DiagnosticCode::kAssetNameDuplicate, "图集重名", "atlases");
      return AtlasId();
    }
  }
  Atlas a;
  a.id = newAtlasId();
  a.name = std::move(name);
  const AtlasId id = a.id;
  data_.atlases.push_back(std::move(a));
  succeed();
  return id;
}

Status Project::addAtlasPage(const AtlasId& atlas, const AtlasPage& page) {
  const std::size_t ai = indexOfAtlas(atlas);
  if (ai == kNpos) return recordFailure(DiagnosticCode::kAssetNotFound, "图集不存在", "atlases");
  const Status s = checkPage(data_, page);
  if (!s.isOk()) { lastStatus_ = s.diagnostic(); return s; }
  // 全局页 ID 唯一性（跨 Atlas；同图集重复同样会被本循环捕获）
  for (const Atlas& a : data_.atlases) {
    for (const AtlasPage& p : a.pages) {
      if (p.id == page.id) return recordFailure(DiagnosticCode::kIdDuplicate, "页 ID 重复", pageLoc(page.id));
    }
  }
  data_.atlases[ai].pages.push_back(page);
  return succeed();
}

Status Project::addAtlasRegion(const AtlasId& atlas, const AtlasRegion& region) {
  const std::size_t ai = indexOfAtlas(atlas);
  if (ai == kNpos) return recordFailure(DiagnosticCode::kAssetNotFound, "图集不存在", "atlases");
  const Status s = checkRegion(data_.atlases[ai], region);
  if (!s.isOk()) { lastStatus_ = s.diagnostic(); return s; }
  // 全局区域 ID 唯一性（跨 Atlas）
  for (const Atlas& a : data_.atlases) {
    for (const AtlasRegion& r : a.regions) {
      if (r.id == region.id)
        return recordFailure(DiagnosticCode::kIdDuplicate, "区域 ID 重复", regionLoc(region.id));
    }
  }
  data_.atlases[ai].regions.push_back(region);
  return succeed();
}

AtlasRegionId Project::adoptImageAssetAsWholeImageRegion(const ImageAssetId& assetId) {
  const ImageAsset* a = findAsset(assetId);
  if (a == nullptr) {
    recordFailure(DiagnosticCode::kAssetNotFound, "资源不存在", assetLoc(assetId));
    return AtlasRegionId();
  }
  const std::int32_t w = a->pixelWidth.value_or(0);
  const std::int32_t h = a->pixelHeight.value_or(0);
  if (w <= 0 || h <= 0) {
    recordFailure(DiagnosticCode::kInvalidRegionRect,
                  "整图区域需要已知像素尺寸", assetLoc(assetId));
    return AtlasRegionId();
  }
  // 兼容映射（Task 1 ImageAsset → Task 2 TextureAsset + Atlas + 整图 Region），
  // 原子性：先在本地构造完整链，校验通过后才一次性提交；
  // 任一步失败都不会在 data_ 留下新建的 TextureAsset / Atlas / Page。
  TextureAsset tex;
  tex.id = newTextureAssetId();
  tex.name = a->name;
  tex.storage = a->storage;
  tex.location = a->location;
  tex.pixelWidth = w;
  tex.pixelHeight = h;

  Atlas at;
  at.id = newAtlasId();
  at.name = a->name + "_atlas";

  AtlasPage page;
  page.id = newAtlasPageId();
  page.name = a->name + "_page";
  page.textureAssetId = tex.id;
  page.width = w;
  page.height = h;
  at.pages.push_back(page);

  AtlasRegion region;
  region.id = newAtlasRegionId();
  region.name = a->name;
  region.pageId = page.id;
  region.packedX = 0;
  region.packedY = 0;
  region.packedW = w;
  region.packedH = h;
  region.rotation = RegionRotation::kNone;
  region.originalW = w;
  region.originalH = h;
  region.trimmedW = w;
  region.trimmedH = h;
  region.offsetX = 0;
  region.offsetY = 0;
  at.regions.push_back(region);

  data_.textures.push_back(std::move(tex));
  data_.atlases.push_back(std::move(at));
  succeed();
  return data_.atlases.back().regions.back().id;
}

const AtlasRegion* Project::findRegion(const AtlasRegionId& id) const {
  for (const Atlas& a : data_.atlases) {
    for (const AtlasRegion& r : a.regions) {
      if (r.id == id) return &r;
    }
  }
  return nullptr;
}

const AtlasPage* Project::findPage(const AtlasPageId& id) const {
  for (const Atlas& a : data_.atlases) {
    for (const AtlasPage& p : a.pages) {
      if (p.id == id) return &p;
    }
  }
  return nullptr;
}

const TextureAsset* Project::findTextureAsset(const TextureAssetId& id) const {
  for (const TextureAsset& t : data_.textures) {
    if (t.id == id) return &t;
  }
  return nullptr;
}

std::vector<const NodeRecord*> Project::nodesReferencingRegion(const AtlasRegionId& id) const {
  std::vector<const NodeRecord*> out;
  for (const NodeRecord& n : data_.nodes) {
    if (n.regionId.has_value() && *n.regionId == id) out.push_back(&n);
  }
  return out;
}

Status Project::deleteAtlasRegion(const AtlasRegionId& id) {
  const std::vector<const NodeRecord*> refs = nodesReferencingRegion(id);
  if (!refs.empty()) {
    return recordFailure(DiagnosticCode::kRegionInUse,
                         "区域仍被 " + std::to_string(refs.size()) + " 个节点引用", regionLoc(id));
  }
  for (Atlas& a : data_.atlases) {
    for (std::size_t i = 0; i < a.regions.size(); ++i) {
      if (a.regions[i].id == id) {
        a.regions.erase(a.regions.begin() + static_cast<std::ptrdiff_t>(i));
        return succeed();
      }
    }
  }
  return recordFailure(DiagnosticCode::kRegionNotFound, "区域不存在", regionLoc(id));
}
// ---- 节点创建 ----

NodeId Project::createGroupNode(const NodeId& parent, GroupNodeData data) {
  const std::size_t pi = indexOfNode(parent);
  if (pi == kNpos) {
    recordFailure(DiagnosticCode::kNodeNotFound, "父节点不存在", nodeLoc(parent));
    return NodeId();
  }
  if (!data.transform.isFinite()) {
    recordFailure(DiagnosticCode::kTransformNotFinite, "变换含非有限数值", nodeLoc(parent));
    return NodeId();
  }
  // Task 2 层级不变量：任何节点（含场景根）都可作为父节点，不因渲染类型拒绝挂载。
  // 根的特殊性只在"根自身没有父节点、身份与变换不可变"，见 moveNode/deleteNode/
  // setNodeTransform 与 validate 中的根检查；**不禁止把节点挂到根下面**。

  NodeRecord rec;
  rec.common.id = Id::generate(IdKind::Node);
  rec.common.name = std::move(data.name);
  rec.common.parent = parent;
  rec.common.transform = data.transform;
  rec.common.visible = data.visible;
  rec.type = NodeType::kGroup;
  rec.assetId = std::nullopt;
  const NodeId newId = rec.common.id;
  data_.nodes.push_back(std::move(rec));
  data_.nodeOrder.push_back(newId);
  data_.nodes[pi].common.children.push_back(newId);
  succeed();
  return newId;
}
NodeId Project::createImageNode(const NodeId& parent, ImageNodeData data) {
  const std::size_t pi = indexOfNode(parent);
  if (pi == kNpos) {
    recordFailure(DiagnosticCode::kNodeNotFound, "父节点不存在", nodeLoc(parent));
    return NodeId();
  }
  if (!data.transform.isFinite()) {
    recordFailure(DiagnosticCode::kTransformNotFinite, "变换含非有限数值", nodeLoc(parent));
    return NodeId();
  }
  if (indexOfAsset(data.assetId) == kNpos) {
    recordFailure(DiagnosticCode::kAssetNotFound, "引用的资源不存在", assetLoc(data.assetId));
    return NodeId();
  }
  // Task 2：图片节点同样可以作为父节点，不再拒绝；根也可以作为父节点。
  NodeRecord rec;
  rec.common.id = Id::generate(IdKind::Node);
  rec.common.name = std::move(data.name);
  rec.common.parent = parent;
  rec.common.transform = data.transform;
  rec.common.visible = data.visible;
  rec.type = NodeType::kImage;
  rec.assetId = data.assetId;

  const NodeId newId = rec.common.id;
  data_.nodes.push_back(std::move(rec));
  data_.nodeOrder.push_back(newId);
  data_.nodes[pi].common.children.push_back(newId);
  succeed();
  return newId;
}

NodeId Project::createSpriteNode(const NodeId& parent, SpriteNodeData data) {
  const std::size_t pi = indexOfNode(parent);
  if (pi == kNpos) {
    recordFailure(DiagnosticCode::kNodeNotFound, "父节点不存在", nodeLoc(parent));
    return NodeId();
  }
  if (!data.transform.isFinite()) {
    recordFailure(DiagnosticCode::kTransformNotFinite, "变换含非有限数值", nodeLoc(parent));
    return NodeId();
  }
  if (findRegion(data.regionId) == nullptr) {
    recordFailure(DiagnosticCode::kRegionNotFound, "引用的区域不存在", regionLoc(data.regionId));
    return NodeId();
  }
  // 节点只存稳定区域 ID，不复制区域几何；同一区域可被多个 Sprite 引用。
  NodeRecord rec;
  rec.common.id = Id::generate(IdKind::Node);
  rec.common.name = std::move(data.name);
  rec.common.parent = parent;
  rec.common.transform = data.transform;
  rec.common.visible = data.visible;
  rec.type = NodeType::kImage;
  rec.regionId = data.regionId;
  rec.assetId = std::nullopt;
  const NodeId newId = rec.common.id;
  data_.nodes.push_back(std::move(rec));
  data_.nodeOrder.push_back(newId);
  data_.nodes[pi].common.children.push_back(newId);
  succeed();
  return newId;
}

Status Project::setNodeRegion(const NodeId& id, const AtlasRegionId& regionId) {
  const std::size_t i = indexOfNode(id);
  if (i == kNpos) {
    return recordFailure(DiagnosticCode::kNodeNotFound, "节点不存在", nodeLoc(id));
  }
  if (data_.nodes[i].type != NodeType::kImage) {
    return recordFailure(DiagnosticCode::kNodeTypeMismatch, "只有图片节点可引用区域", nodeLoc(id));
  }
  if (findRegion(regionId) == nullptr) {
    return recordFailure(DiagnosticCode::kRegionNotFound, "引用的区域不存在", regionLoc(regionId));
  }
  data_.nodes[i].regionId = regionId;
  data_.nodes[i].assetId = std::nullopt;  // 切换为区域引用时清除资产引用
  return succeed();
}

// ---- 节点属性 ----

Status Project::renameNode(const NodeId& id, std::string newName) {
  const std::size_t i = indexOfNode(id);
  if (i == kNpos) {
    return recordFailure(DiagnosticCode::kNodeNotFound, "节点不存在", nodeLoc(id));
  }
  data_.nodes[i].common.name = std::move(newName);
  return succeed();
}
Status Project::setNodeTransform(const NodeId& id, const Transform& transform) {
  const std::size_t i = indexOfNode(id);
  if (i == kNpos) {
    return recordFailure(DiagnosticCode::kNodeNotFound, "节点不存在", nodeLoc(id));
  }
  if (!transform.isFinite()) {
    return recordFailure(DiagnosticCode::kTransformNotFinite, "变换含 NaN 或 Inf", nodeLoc(id));
  }
  // 场景根必须保持恒等局部变换（Task 2 §4.1.2）
  if (data_.nodes[i].type == NodeType::kRoot && !(transform == Transform{})) {
    return recordFailure(DiagnosticCode::kRootImmutable, "场景根的局部变换不可修改", nodeLoc(id));
  }
  data_.nodes[i].common.transform = transform;
  return succeed();
}
Status Project::setNodeVisible(const NodeId& id, bool visible) {
  const std::size_t i = indexOfNode(id);
  if (i == kNpos) {
    return recordFailure(DiagnosticCode::kNodeNotFound, "节点不存在", nodeLoc(id));
  }
  data_.nodes[i].common.visible = visible;
  return succeed();
}

Status Project::renameProject(std::string newName) {
  data_.name = std::move(newName);
  return succeed();
}

Status Project::setNodeLocked(const NodeId& id, bool locked) {
  const std::size_t i = indexOfNode(id);
  if (i == kNpos) {
    return recordFailure(DiagnosticCode::kNodeNotFound, "节点不存在", nodeLoc(id));
  }
  data_.nodes[i].common.locked = locked;
  return succeed();
}

// ---- 场景 ----

const Scene* Project::defaultScene() const {
  if (data_.scenes.empty()) return nullptr;
  if (data_.defaultSceneIndex >= data_.scenes.size()) return nullptr;
  return &data_.scenes[data_.defaultSceneIndex];
}

const Scene* Project::findScene(const SceneId& id) const {
  for (const Scene& s : data_.scenes) {
    if (s.id == id) return &s;
  }
  return nullptr;
}

Status Project::setDefaultScene(std::size_t index) {
  if (index >= data_.scenes.size()) {
    return recordFailure(DiagnosticCode::kSceneNotFound, "场景下标越界", "scenes");
  }
  data_.defaultSceneIndex = index;
  data_.rootNodeId = data_.scenes[index].rootNodeId;
  return succeed();
}

// ---- 绘制顺序：先自身，再按 children 有序递归 ----

std::vector<const NodeRecord*> Project::drawOrder() const {
  std::vector<const NodeRecord*> out;
  std::unordered_set<std::string> visited;
  const Scene* sc = defaultScene();
  const NodeId start = (sc != nullptr) ? sc->rootNodeId : data_.rootNodeId;
  std::vector<const NodeRecord*> stack;
  if (const NodeRecord* r = findNode(start)) stack.push_back(r);
  while (!stack.empty()) {
    const NodeRecord* cur = stack.back();
    stack.pop_back();
    if (!visited.insert(cur->common.id.str()).second) continue;
    out.push_back(cur);
    // children 是**逆序**压栈，保证弹出顺序与 children 顺序一致
    for (std::size_t i = cur->common.children.size(); i-- > 0;) {
      if (const NodeRecord* c = findNode(cur->common.children[i])) stack.push_back(c);
    }
  }
  // 兜底：未从根可达的节点（非法数据）也要有确定位置，放在最后并去重
  for (const NodeRecord& n : data_.nodes) {
    if (visited.insert(n.common.id.str()).second) out.push_back(&n);
  }
  return out;
}

std::vector<const NodeRecord*> Project::drawableOrder() const {
  std::vector<const NodeRecord*> all = drawOrder();
  std::vector<const NodeRecord*> out;
  out.reserve(all.size());
  for (const NodeRecord* n : all) {
    if (n->type != NodeType::kRoot) out.push_back(n);  // 根不参与绘制
  }
  return out;
}
// ---- 移动与顺序 ----

Status Project::moveNode(const NodeId& id, const NodeId& newParent, std::size_t index) {
  const std::size_t ni = indexOfNode(id);
  if (ni == kNpos) {
    return recordFailure(DiagnosticCode::kNodeNotFound, "节点不存在", nodeLoc(id));
  }
  // 场景根不可移动：它固定在原点，是树的唯一锚点
  if (data_.nodes[ni].type == NodeType::kRoot || id == data_.rootNodeId) {
    return recordFailure(DiagnosticCode::kRootImmutable, "场景根不能被移动", nodeLoc(id));
  }
  const std::size_t pi = indexOfNode(newParent);
  if (pi == kNpos) {
    return recordFailure(DiagnosticCode::kNodeNotFound, "目标父节点不存在", nodeLoc(newParent));
  }
  // Task 2：目标父节点可以是任何节点（含场景根）
  if (isDescendantOf(newParent, id)) {
    return recordFailure(DiagnosticCode::kCannotMoveIntoSelf,
                         "不能把节点移入自身或其子树", nodeLoc(id));
  }

  // 先算好目标位置再提交, 避免中途失败留下半完成状态
    const std::size_t oldPi = indexOfNode(data_.nodes[ni].common.parent);
  if (oldPi != kNpos) detachChild(data_.nodes[oldPi].common, id);
  std::vector<NodeId>& newSibs = data_.nodes[pi].common.children;
  const std::size_t pos = std::min(index, newSibs.size());
  newSibs.insert(newSibs.begin() + static_cast<std::ptrdiff_t>(pos), id);
  data_.nodes[ni].common.parent = newParent;
  return succeed();
}
Status Project::reorderChild(const NodeId& parent, std::size_t fromIndex, std::size_t toIndex) {
  const std::size_t pi = indexOfNode(parent);
  if (pi == kNpos) {
    return recordFailure(DiagnosticCode::kNodeNotFound, "分组不存在", nodeLoc(parent));
  }
  // Task 2：任何父节点（含场景根）的子节点列表都可排序
  std::vector<NodeId>& kids = data_.nodes[pi].common.children;
  if (fromIndex >= kids.size() || toIndex >= kids.size()) {
    return recordFailure(DiagnosticCode::kNodeNotFound, "下标越界", nodeLoc(parent));
  }
  if (fromIndex == toIndex) return succeed();
  const NodeId moved = kids[fromIndex];
  kids.erase(kids.begin() + static_cast<std::ptrdiff_t>(fromIndex));
  kids.insert(kids.begin() + static_cast<std::ptrdiff_t>(toIndex), moved);
  return succeed();
}
Status Project::deleteNode(const NodeId& id, ChildPolicy policy) {
  const std::size_t ni = indexOfNode(id);
  if (ni == kNpos) {
    return recordFailure(DiagnosticCode::kNodeNotFound, "节点不存在", nodeLoc(id));
  }
  if (data_.nodes[ni].type == NodeType::kRoot || id == data_.rootNodeId) {
    return recordFailure(DiagnosticCode::kRootImmutable, "场景根不可删除", nodeLoc(id));
  }
  const std::size_t childCount = data_.nodes[ni].common.children.size();
  if (childCount > 0 && policy == ChildPolicy::kRejectIfHasChildren) {
    return recordFailure(DiagnosticCode::kNodeHasChildren,
                         "节点还有 " + std::to_string(childCount) + " 个子节点", nodeLoc(id));
  }
  // 先收集要删除的子树, 再统一提交, 避免中途失败
  std::vector<NodeId> doomed;
  if (policy == ChildPolicy::kDeleteSubtree) {
    std::vector<const NodeRecord*> stack{&data_.nodes[ni]};
    std::unordered_set<std::string> seen;
    while (!stack.empty()) {
      const NodeRecord* cur = stack.back();
      stack.pop_back();
      if (!seen.insert(cur->common.id.str()).second) continue;
      doomed.push_back(cur->common.id);
      for (const NodeId& c : cur->common.children) {
        const std::size_t ci = indexOfNode(c);
        if (ci != kNpos) stack.push_back(&data_.nodes[ci]);
      }
    }
  } else {
    doomed.push_back(id);
  }
  const NodeId oldParent = data_.nodes[ni].common.parent;
  const std::size_t opi = indexOfNode(oldParent);
  if (opi != kNpos) detachChild(data_.nodes[opi].common, id);

  std::unordered_set<std::string> kill;
  for (const NodeId& d : doomed) kill.insert(d.str());
  std::vector<NodeRecord> kept;
  std::vector<NodeId> keptOrder;
  for (std::size_t i = 0; i < data_.nodes.size(); ++i) {
    if (kill.count(data_.nodes[i].common.id.str()) != 0) continue;
    kept.push_back(data_.nodes[i]);
    keptOrder.push_back(data_.nodeOrder[i]);
  }
  data_.nodes = std::move(kept);
  data_.nodeOrder = std::move(keptOrder);
  return succeed();
}
// ---- 校验（Task1 4 的十条规则）----
std::vector<Diagnostic> Project::validate() const {
  std::vector<Diagnostic> out;
  const auto add = [&out](DiagnosticCode c, std::string m, std::string loc) {
    out.emplace_back(c, std::move(m), std::move(loc));
  };
  if (!data_.id.isWellFormed()) {
    add(DiagnosticCode::kIdMalformed, "工程 ID 格式非法", "project");
  }

  // ---- 场景与根（Task 2 Back Task 2.1）：单场景契约 ----
  // 仅支持一个 Scene；其 rootNodeId 必须指向存在的 kRoot 节点，且与兼容字段
  // ProjectData::rootNodeId 完全一致。场景 ID 合法且唯一。
  if (data_.scenes.empty()) {
    add(DiagnosticCode::kSceneNotFound, "工程至少需要一个场景", "scenes");
  } else if (data_.scenes.size() != 1) {
    add(DiagnosticCode::kSceneCountMismatch, "仅支持单一场景", "scenes");
  } else {
    const Scene& sc = data_.scenes[0];
    if (!sc.id.isWellFormed()) {
      add(DiagnosticCode::kIdMalformed, "场景 ID 非法", "scenes");
    } else if (sc.id.kind() != IdKind::Scene) {
      add(DiagnosticCode::kIdKindMismatch, "场景 ID 种类错误", sceneLoc(sc.id));
    }
    if (data_.defaultSceneIndex != 0) {
      add(DiagnosticCode::kSceneNotFound, "单场景契约下默认场景下标必须为 0", "scenes");
    }
    const NodeRecord* sceneRoot = findNode(sc.rootNodeId);
    if (sceneRoot == nullptr) {
      add(DiagnosticCode::kSceneRootInvalid, "场景根指向缺失节点", sceneLoc(sc.id));
    } else if (sceneRoot->type != NodeType::kRoot) {
      add(DiagnosticCode::kSceneRootInvalid, "场景根必须是 kRoot 节点", nodeLoc(sc.rootNodeId));
    }
    if (data_.rootNodeId != sc.rootNodeId) {
      add(DiagnosticCode::kSceneRootInvalid, "默认场景根与兼容字段 rootNodeId 不一致", "scenes");
    }
  }

  // ---- 根的唯一特殊身份：全工程恰好一个 kRoot ----
  // 无父节点、恒等局部变换、不持有任何内容引用（ImageAsset / AtlasRegion）。
  {
    std::size_t rootCount = 0;
    const NodeRecord* rootNode = nullptr;
    for (const NodeRecord& n : data_.nodes) {
      if (n.type == NodeType::kRoot) {
        ++rootCount;
        rootNode = &n;
      }
    }
    if (rootCount == 0) {
      add(DiagnosticCode::kRootMissing, "缺少 kRoot 根节点", "nodes");
    } else if (rootCount > 1) {
      add(DiagnosticCode::kRootNotUnique, "工程中存在多个根节点", "nodes");
    } else if (rootNode != nullptr) {
      if (!rootNode->common.parent.isNone()) {
        add(DiagnosticCode::kRootParented, "根节点不应有父节点", nodeLoc(rootNode->common.id));
      }
      if (!(rootNode->common.transform == Transform{})) {
        add(DiagnosticCode::kRootImmutable, "场景根必须保持恒等局部变换", nodeLoc(rootNode->common.id));
      }
      if (rootNode->assetId.has_value() || rootNode->regionId.has_value()) {
        add(DiagnosticCode::kNodeTypeMismatch, "场景根不持有图片内容", nodeLoc(rootNode->common.id));
      }
    }
  }

  std::unordered_set<std::string> nodeIds;
  for (const NodeRecord& n : data_.nodes) {
    if (!n.common.id.isWellFormed()) {
      add(DiagnosticCode::kIdMalformed, "节点 ID 非法", nodeLoc(n.common.id));
    } else if (n.common.id.kind() != IdKind::Node) {
      add(DiagnosticCode::kIdKindMismatch, "节点 ID 种类错误", nodeLoc(n.common.id));
    } else if (!nodeIds.insert(n.common.id.str()).second) {
      add(DiagnosticCode::kIdDuplicate, "节点 ID 重复", nodeLoc(n.common.id));
    }
  }
  std::unordered_set<std::string> assetIds;
  for (const ImageAsset& a : data_.assets) {
    if (!a.id.isWellFormed()) {
      add(DiagnosticCode::kIdMalformed, "资源 ID 非法", assetLoc(a.id));
    } else if (a.id.kind() != IdKind::ImageAsset) {
      add(DiagnosticCode::kIdKindMismatch, "资源 ID 种类错误", assetLoc(a.id));
    } else if (!assetIds.insert(a.id.str()).second) {
      add(DiagnosticCode::kIdDuplicate, "资源 ID 重复", assetLoc(a.id));
    }
  }
  if (data_.nodes.size() != data_.nodeOrder.size()) {
    add(DiagnosticCode::kProjectInvalid, "nodes 与 nodeOrder 长度不一致", "");
  }
  // 规则 9 + 10
  for (const NodeRecord& n : data_.nodes) {
    if (!n.typeMatchesData()) {
      add(DiagnosticCode::kNodeTypeMismatch, "类型与数据结构不一致", nodeLoc(n.common.id));
    }
    if (!n.common.transform.isFinite()) {
      add(DiagnosticCode::kTransformNotFinite, "变换含 NaN 或 Inf", nodeLoc(n.common.id));
    }
  }
  // 规则 6 + 7：图片节点必须持有「资产或区域」之一；分组与场景根都不持有
  for (const NodeRecord& n : data_.nodes) {
    if (n.type == NodeType::kImage) {
      const bool okAsset = n.assetId.has_value() && indexOfAsset(*n.assetId) != kNpos;
      const bool okRegion = n.regionId.has_value() && findRegion(*n.regionId) != nullptr;
      if (!okAsset && !okRegion) {
        add(DiagnosticCode::kAssetNotFound, "图片节点未引用存在的资产或区域", nodeLoc(n.common.id));
      }
    } else if (n.assetId.has_value()) {
      add(DiagnosticCode::kGroupCannotHoldAsset,
          n.type == NodeType::kRoot ? "场景根不得引用资源" : "分组不得引用资源",
          nodeLoc(n.common.id));
    }
  }

  // 规则 3 + 4: 父子双向一致
  std::unordered_map<std::string, std::size_t> backRefs;
  for (const NodeRecord& n : data_.nodes) {
    std::unordered_set<std::string> seenChild;
    for (const NodeId& c : n.common.children) {
      backRefs[c.str()] += 1;
      if (!seenChild.insert(c.str()).second) {
        add(DiagnosticCode::kDuplicateChild, "同一父节点下出现重复子引用", nodeLoc(n.common.id));
      }
      const NodeRecord* cn = findNode(c);
      if (cn == nullptr) {
        add(DiagnosticCode::kParentChildMismatch, "子节点不存在", nodeLoc(n.common.id));
      } else if (cn->common.parent != n.common.id) {
        add(DiagnosticCode::kParentChildMismatch, "父子关系不一致", nodeLoc(c));
      } else if (cn->type == NodeType::kRoot) {
        add(DiagnosticCode::kRootParented, "场景根不能被挂载为子节点", nodeLoc(c));
      }
    }
  }

  // ---- 图集资源层校验（Task 2 Back Task 2.2）：与写 API 共用 checkPage/checkRegion ----
  // 节点引用区域闭合：仅图片节点可引用；区域必须存在
  for (const NodeRecord& n : data_.nodes) {
    if (n.regionId.has_value()) {
      if (n.type != NodeType::kImage) {
        add(DiagnosticCode::kNodeTypeMismatch, "只有图片节点可引用区域", nodeLoc(n.common.id));
      } else if (findRegion(*n.regionId) == nullptr) {
        add(DiagnosticCode::kRegionNotFound, "引用的图集区域不存在", nodeLoc(n.common.id));
      }
    }
  }
  for (const Atlas& a : data_.atlases) {
    if (!a.id.isWellFormed()) {
      add(DiagnosticCode::kIdMalformed, "图集 ID 非法", atlasLoc(a.id));
    } else if (a.id.kind() != IdKind::Atlas) {
      add(DiagnosticCode::kIdKindMismatch, "图集 ID 种类错误", atlasLoc(a.id));
    }
    for (const AtlasPage& p : a.pages) {
      const Status sp = checkPage(data_, p);
      if (!sp.isOk()) {
        const Diagnostic& d = sp.diagnostic();
        add(d.code, d.message, d.location);
      }
    }
    for (const AtlasRegion& r : a.regions) {
      const Status sr = checkRegion(a, r);
      if (!sr.isOk()) {
        const Diagnostic& d = sr.diagnostic();
        add(d.code, d.message, d.location);
      }
    }
  }
  // 各资源类型 ID 的全局唯一性（跨 Atlas）
  std::unordered_set<std::string> textureIds, atlasIds, pageIds, regionIds;
  for (const TextureAsset& t : data_.textures) {
    if (!t.id.isWellFormed()) {
      add(DiagnosticCode::kIdMalformed, "纹理资产 ID 非法", textureLoc(t.id));
    } else if (t.id.kind() != IdKind::TextureAsset) {
      add(DiagnosticCode::kIdKindMismatch, "纹理资产 ID 种类错误", textureLoc(t.id));
    } else if (!textureIds.insert(t.id.str()).second) {
      add(DiagnosticCode::kIdDuplicate, "纹理资产 ID 重复", textureLoc(t.id));
    }
  }
  for (const Atlas& a : data_.atlases) {
    if (!atlasIds.insert(a.id.str()).second) {
      add(DiagnosticCode::kIdDuplicate, "图集 ID 重复", atlasLoc(a.id));
    }
    for (const AtlasPage& p : a.pages) {
      if (!pageIds.insert(p.id.str()).second) {
        add(DiagnosticCode::kIdDuplicate, "页 ID 全局重复", pageLoc(p.id));
      }
    }
    for (const AtlasRegion& r : a.regions) {
      if (!regionIds.insert(r.id.str()).second) {
        add(DiagnosticCode::kIdDuplicate, "区域 ID 全局重复", regionLoc(r.id));
      }
    }
  }
  for (const NodeRecord& n : data_.nodes) {
    if (n.common.id == data_.rootNodeId) {
      if (!n.common.parent.isNone()) {
        add(DiagnosticCode::kRootParented, "根节点不应有父节点", nodeLoc(n.common.id));
      }
      continue;
    }
    if (findNode(n.common.parent) == nullptr) {
      add(DiagnosticCode::kNodeOrphan, "非根节点缺父节点", nodeLoc(n.common.id));
    } else if (backRefs[n.common.id.str()] > 1) {
      add(DiagnosticCode::kNodeHasParent, "节点被多个父节点引用", nodeLoc(n.common.id));
    }
  }
  // 规则 5: 环检测（沿 parent 链上溯, 超过节点数即成环）
  for (const NodeRecord& n : data_.nodes) {
    std::unordered_set<std::string> seen{n.common.id.str()};
    const NodeRecord* cur = &n;
    std::size_t steps = 0;
    while (cur != nullptr && cur->common.id != data_.rootNodeId) {
      if (!seen.insert(cur->common.parent.str()).second) {
        add(DiagnosticCode::kCycleDetected, "存在循环层级", nodeLoc(n.common.id));
        break;
      }
      cur = findNode(cur->common.parent);
      if (++steps > data_.nodes.size()) {
        add(DiagnosticCode::kCycleDetected, "存在循环层级", nodeLoc(n.common.id));
        break;
      }
    }
  }

  // ---- 骨架层校验（Task 4 B2）----
  // 骨架 ID 自身的形态 / 种类 / 跨骨架唯一性在此处理；骨架内部规则
  // （根骨、父引用、排列、slot→bone、skin→slot、entry 唯一、非有限值等）
  // 集中在 validateSkeleton()，与将来的骨架写 API 共用同一套判据。
  std::unordered_set<std::string> skeletonIds;
  skeletonIds.reserve(data_.skeletons.size() * 2U + 1U);
  for (const Skeleton& s : data_.skeletons) {
    const std::string where = "skeletons/" + s.id.str();
    if (!s.id.isWellFormed()) {
      add(DiagnosticCode::kIdMalformed, "骨架 ID 非法", where);
    } else if (s.id.kind() != IdKind::Skeleton) {
      add(DiagnosticCode::kIdKindMismatch, "骨架 ID 种类错误", where);
    } else if (!skeletonIds.insert(s.id.str()).second) {
      add(DiagnosticCode::kIdDuplicate, "骨架 ID 重复", where);
    }
    for (const Diagnostic& d : validateSkeleton(s, data_)) {
      add(d.code, d.message, d.location);
    }
  }
  return out;
}

// ---- 导入装配 ----

Status Project::adoptRoot(const NodeId& groupNodeId) {
  const NodeRecord* n = findNode(groupNodeId);
  if (n == nullptr) {
    return recordFailure(DiagnosticCode::kNodeNotFound, "根节点不存在", nodeLoc(groupNodeId));
  }
  // Task 2：根是明确类型 kRoot，避免"任意 GroupNode 都能变成根"。
  if (n->type != NodeType::kRoot) {
    return recordFailure(DiagnosticCode::kRootNotGroup, "根必须是 kRoot 类型", nodeLoc(groupNodeId));
  }
  if (!data_.scenes.empty()) {
    data_.scenes[data_.defaultSceneIndex].rootNodeId = groupNodeId;
  }
  data_.rootNodeId = groupNodeId;  // Task 1 兼容字段
  return succeed();
}

Status Project::adoptData(ProjectData data) {
  // 原子性：先在候选对象上整体校验，只有全部通过才提交。
  // 校验失败（含图片节点带子节点这类非法结构）时原 Project 数据原样保留。
  Project candidate;
  candidate.data_ = std::move(data);
  const std::vector<Diagnostic> problems = candidate.validate();
  if (!problems.empty()) {
    lastStatus_ = problems.front();
    return Status::fail(problems.front().code, problems.front().message,
                        problems.front().location);
  }
  data_ = std::move(candidate.data_);
  return succeed();
}

}
  // namespace loongbones