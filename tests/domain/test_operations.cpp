// 基本操作用例：移动、顺序调整、删除策略、资源引用保护。

#include "test_framework.hpp"

#include <vector>

#include "loongbones/domain/project.hpp"

using namespace loongbones;

namespace {
NodeId addGroup(Project& p, const NodeId& parent, const char* name) {
  GroupNodeData d{name, Transform(), true};
  return p.createGroupNode(parent, d);
}
std::vector<NodeId> childIds(const Project& p, const NodeId& parent) {
  std::vector<NodeId> out;
  for (const NodeRecord* n : p.childrenOf(parent)) out.push_back(n->common.id);
  return out;
}
}  // namespace
TEST(Operations, MoveBetweenGroups) {
  Project p = Project::create("demo");
  const NodeId g1 = addGroup(p, p.rootNodeId(), "g1");
  const NodeId g2 = addGroup(p, p.rootNodeId(), "g2");
  const NodeId n = addGroup(p, g1, "leaf");
  CHECK(p.moveNode(n, g2, 0).isOk());
  CHECK_EQ(childIds(p, g1).size(), std::size_t{0});
  CHECK_EQ(childIds(p, g2).size(), std::size_t{1});
  CHECK_EQ(p.parentOf(n)->common.id, g2);
  CHECK(p.validate().empty());
}

TEST(Operations, MoveRejectsSelfAndDescendant) {
  Project p = Project::create("demo");
  const NodeId g = addGroup(p, p.rootNodeId(), "g");
  const NodeId c = addGroup(p, g, "c");
  CHECK(!p.moveNode(g, g, 0).isOk());
  CHECK(!p.moveNode(g, c, 0).isOk());
  CHECK(p.validate().empty());
}
TEST(Operations, ReorderWithinGroup) {
  Project p = Project::create("demo");
  const NodeId a = addGroup(p, p.rootNodeId(), "a");
  const NodeId b = addGroup(p, p.rootNodeId(), "b");
  const NodeId c = addGroup(p, p.rootNodeId(), "c");
  CHECK(p.reorderChild(p.rootNodeId(), 0, 2).isOk());
  const std::vector<NodeId> ids = childIds(p, p.rootNodeId());
  CHECK_EQ(ids[0], b);
  CHECK_EQ(ids[1], c);
  CHECK_EQ(ids[2], a);
}
TEST(Operations, DeleteRequiresExplicitPolicy) {
  Project p = Project::create("demo");
  const NodeId g = addGroup(p, p.rootNodeId(), "g");
  addGroup(p, g, "child");
  CHECK(!p.deleteNode(g, ChildPolicy::kRejectIfHasChildren).isOk());
  CHECK(p.findNode(g) != nullptr);
  CHECK(p.deleteNode(g, ChildPolicy::kDeleteSubtree).isOk());
  CHECK(p.findNode(g) == nullptr);
  CHECK(p.validate().empty());
}

TEST(Operations, RootCannotBeDeletedOrMoved) {
  Project p = Project::create("demo");
  const NodeId g = addGroup(p, p.rootNodeId(), "g");
  CHECK(!p.deleteNode(p.rootNodeId(), ChildPolicy::kDeleteSubtree).isOk());
  CHECK(!p.moveNode(p.rootNodeId(), g, 0).isOk());
}
TEST(Operations, AssetDeleteProtectedWhileReferenced) {
  Project p = Project::create("demo");
  const ImageAssetId asset = p.createImageAsset("hero.png");
  ImageNodeData d{"hero", asset, Transform(), true};
  const NodeId n = p.createImageNode(p.rootNodeId(), d);
  CHECK(!p.deleteImageAsset(asset).isOk());
  CHECK(p.findAsset(asset) != nullptr);
  CHECK(p.deleteNode(n, ChildPolicy::kDeleteSubtree).isOk());
  CHECK(p.deleteImageAsset(asset).isOk());
  CHECK(p.findAsset(asset) == nullptr);
}
TEST(Operations, DuplicateAssetNameRejected) {
  Project p = Project::create("demo");
  CHECK(p.createImageAsset("hero.png").isWellFormed());
  CHECK(!p.createImageAsset("hero.png").isWellFormed());
}

TEST(Operations, MissingParentAndAssetRejected) {
  Project p = Project::create("demo");
  GroupNodeData g{"g", Transform(), true};
  const NodeId missing = Id::generate(IdKind::Node);
  CHECK(!p.createGroupNode(missing, g).isWellFormed());
  ImageNodeData d{"x", ImageAssetId::generate(IdKind::ImageAsset), Transform(), true};
  CHECK(!p.createImageNode(p.rootNodeId(), d).isWellFormed());
  CHECK(p.validate().empty());
}