// 模型用例：空工程与根、分组嵌套、资源与节点分离、变换与可见性。

#include "test_framework.hpp"

#include <vector>

#include "loongbones/domain/project.hpp"

using namespace loongbones;

TEST(Model, EmptyProjectHasSceneRoot) {
  Project p = Project::create("demo");
  CHECK(p.id().isWellFormed());
  CHECK_EQ(p.name(), std::string("demo"));
  const NodeRecord* root = p.findNode(p.rootNodeId());
  CHECK(root != nullptr);
  // Task 2：根是明确的 kRoot 类型，不是普通分组
  CHECK(root->type == NodeType::kRoot);
  CHECK(root->common.parent.isNone());
  CHECK(root->common.transform == Transform{});  // 恒等
  CHECK(!root->assetId.has_value());
  CHECK(p.childrenOf(p.rootNodeId()).empty());
  CHECK(p.validate().empty());
}
TEST(Model, NestedGroups) {
  Project p = Project::create("demo");
  GroupNodeData g1{"g1", Transform(), true};
  const NodeId a = p.createGroupNode(p.rootNodeId(), g1);
  CHECK(a.isWellFormed());
  GroupNodeData g2{"g2", Transform(), true};
  const NodeId b = p.createGroupNode(a, g2);
  CHECK(b.isWellFormed());
  CHECK_EQ(p.childrenOf(a).size(), std::size_t{1});
  CHECK(p.parentOf(b) != nullptr);
  CHECK_EQ(p.parentOf(b)->common.id, a);
  CHECK(p.isDescendantOf(b, p.rootNodeId()));
  CHECK(p.validate().empty());
}
TEST(Model, AssetAndNodeSeparated) {
  Project p = Project::create("demo");
  const ImageAssetId asset = p.createImageAsset("hero.png");
  CHECK(asset.isWellFormed());
  ImageNodeData d1{"hero_a", asset, Transform(), true};
  ImageNodeData d2{"hero_b", asset, Transform(), true};
  const NodeId n1 = p.createImageNode(p.rootNodeId(), d1);
  const NodeId n2 = p.createImageNode(p.rootNodeId(), d2);
  CHECK(n1 != n2);
  CHECK_EQ(p.nodesReferencing(asset).size(), std::size_t{2});
  CHECK(p.validate().empty());
}
TEST(Model, TransformAndVisibility) {
  Project p = Project::create("demo");
  GroupNodeData g{"g", Transform(), true};
  const NodeId n = p.createGroupNode(p.rootNodeId(), g);
  Transform t;
  t.x = 12.5; t.y = -3.0; t.rotationDeg = 45.0; t.scaleX = 2.0; t.scaleY = 0.5;
  CHECK(p.setNodeTransform(n, t).isOk());
  const NodeRecord* rec = p.findNode(n);
  CHECK(rec != nullptr);
  CHECK_EQ(rec->common.transform.x, 12.5);
  CHECK(p.setNodeVisible(n, false).isOk());
  CHECK_EQ(p.findNode(n)->common.visible, false);
}
TEST(Model, RenameKeepsId) {
  Project p = Project::create("demo");
  GroupNodeData g{"old", Transform(), true};
  const NodeId n = p.createGroupNode(p.rootNodeId(), g);
  const std::string idBefore = n.str();
  CHECK(p.renameNode(n, "new").isOk());
  CHECK_EQ(n.str(), idBefore);
  CHECK_EQ(p.findNode(n)->common.name, std::string("new"));
}