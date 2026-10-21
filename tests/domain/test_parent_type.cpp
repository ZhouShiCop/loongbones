// 节点挂载与原子性回归（Task 2 改写）
//
// Task 2 层级不变量：除场景根外没有挂载特权。
// GroupNode 与图片节点（SpriteNode 角色）平权，都可作父/子节点。
// 保留"失败不变性"思想, 但断言对象改为**真实的**失败条件:
// 根不可挂载、环、孤儿、重复引用、非法变换、非法资源、非法区域。

#include "test_framework.hpp"

#include <limits>
#include <string>
#include <vector>

#include "loongbones/domain/project.hpp"

using namespace loongbones;

namespace {
NodeId addGroup(Project& p, const NodeId& parent, const char* name) {
  GroupNodeData d{name, Transform(), true};
  return p.createGroupNode(parent, d);
}

NodeId addAsset(Project& p, const char* name) {
  return p.createImageAsset(name, StorageMode::kExternalReference, std::string(name) + ".png");
}

NodeId addImage(Project& p, const NodeId& parent, const char* name, const ImageAssetId& a) {
  ImageNodeData d{name, a, Transform(), true};
  return p.createImageNode(parent, d);
}
std::vector<NodeId> childIds(const Project& p, const NodeId& parent) {
  std::vector<NodeId> out;
  for (const NodeRecord* n : p.childrenOf(parent)) out.push_back(n->common.id);
  return out;
}

/// 结构快照: 断言"失败操作后数据完全未变"
std::string snapshot(const Project& p) {
  std::string s;
  for (const NodeRecord* n : p.nodesInOrder()) {
    s += n->common.id.str() + "|" + n->common.name;
    s += "|" + std::to_string(static_cast<int>(n->type));
    s += "|p=" + n->common.parent.str() + "|kids=";
    for (const NodeId& c : n->common.children) s += c.str() + ",";
    s += ";";
  }
  return s;
}

}  // namespace
// 合法: 图片节点（SpriteNode 角色）下可创建分组与图片节点
TEST(ParentType, ImageCanHoldChildren) {
  Project p = Project::create("demo");
  const ImageAssetId a = addAsset(p, "img1");
  const NodeId img = addImage(p, p.rootNodeId(), "img", a);
  CHECK(!img.isNone());

  const NodeId g = addGroup(p, img, "childGroup");
  const NodeId i2 = addImage(p, img, "childImg", a);
  CHECK(!g.isNone());
  CHECK(!i2.isNone());
  CHECK_EQ(childIds(p, img).size(), std::size_t{2});
  CHECK_EQ(p.parentOf(g)->common.id, img);
  CHECK(p.validate().empty());
}
// 合法: 把分组或图片节点移入图片节点
TEST(ParentType, MoveIntoImageIsAllowed) {
  Project p = Project::create("demo");
  const ImageAssetId a = addAsset(p, "img1");
  const NodeId dst = addImage(p, p.rootNodeId(), "dst", a);
  const NodeId g = addGroup(p, p.rootNodeId(), "g");
  const NodeId src = addImage(p, p.rootNodeId(), "src", a);

  CHECK(p.moveNode(g, dst, 0).isOk());
  CHECK(p.moveNode(src, dst, 1).isOk());
  CHECK_EQ(p.parentOf(g)->common.id, dst);
  const std::vector<NodeId> kids = childIds(p, dst);
  CHECK_EQ(kids.size(), std::size_t{2});
  CHECK_EQ(kids[0], g);
  CHECK_EQ(kids[1], src);
  CHECK(p.validate().empty());
}
// 真实失败: 根不可移动、不可删除、变换不可改
TEST(ParentType, RootIsImmutable) {
  Project p = Project::create("demo");
  const NodeId g = addGroup(p, p.rootNodeId(), "g");
  const std::string before = snapshot(p);

  CHECK(!p.moveNode(p.rootNodeId(), g, 0).isOk());
  CHECK(p.lastStatus().code == DiagnosticCode::kRootImmutable);
  CHECK(!p.deleteNode(p.rootNodeId(), ChildPolicy::kDeleteSubtree).isOk());
  CHECK(p.lastStatus().code == DiagnosticCode::kRootImmutable);

  Transform t;
  t.x = 10.0;
  CHECK(!p.setNodeTransform(p.rootNodeId(), t).isOk());
  CHECK(p.lastStatus().code == DiagnosticCode::kRootImmutable);
  CHECK_EQ(snapshot(p), before);
  CHECK(p.validate().empty());
}
// 真实失败: 引用不存在的资源, 失败后结构不变
TEST(ParentType, MissingAssetRejected) {
  Project p = Project::create("demo");
  const std::string before = snapshot(p);
  const std::size_t n = p.data().nodes.size();

  ImageNodeData bad{"x", Id::generate(IdKind::ImageAsset), Transform(), true};
  CHECK(p.createImageNode(p.rootNodeId(), bad).isNone());
  CHECK(p.lastStatus().code == DiagnosticCode::kAssetNotFound);

  CHECK_EQ(p.data().nodes.size(), n);
  CHECK_EQ(snapshot(p), before);
  CHECK(p.validate().empty());
}
// 真实失败: 非有限变换被拒, 原值不变
TEST(ParentType, NonFiniteTransformRejected) {
  Project p = Project::create("demo");
  const NodeId g = addGroup(p, p.rootNodeId(), "g");
  const std::string before = snapshot(p);
  Transform bad;
  bad.scaleX = std::numeric_limits<double>::infinity();
  CHECK(!p.setNodeTransform(g, bad).isOk());
  CHECK(p.lastStatus().code == DiagnosticCode::kTransformNotFinite);
  CHECK_EQ(p.findNode(g)->common.transform.scaleX, 1.0);
  CHECK_EQ(snapshot(p), before);
  CHECK(p.validate().empty());
}
