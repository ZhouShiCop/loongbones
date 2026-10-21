// 原子性用例：失败的写操作不得留下半完成状态（Task1 7）。

#include "test_framework.hpp"

#include <limits>
#include <vector>

#include "loongbones/domain/project.hpp"

using namespace loongbones;

namespace {
std::size_t countNodes(const Project& p) { return p.data().nodes.size(); }
std::size_t countAssets(const Project& p) { return p.data().assets.size(); }
}  // namespace

TEST(Atomicity, FailedCreateLeavesNoNode) {
  Project p = Project::create("demo");
  const std::size_t before = countNodes(p);
  GroupNodeData g{"g", Transform(), true};
  p.createGroupNode(Id::generate(IdKind::Node), g);
  CHECK_EQ(countNodes(p), before);
  CHECK(p.validate().empty());
}
TEST(Atomicity, FailedImageNodeCreateLeavesNoNode) {
  Project p = Project::create("demo");
  const std::size_t before = countNodes(p);
  ImageNodeData d{"x", Id::generate(IdKind::ImageAsset), Transform(), true};
  p.createImageNode(p.rootNodeId(), d);
  CHECK_EQ(countNodes(p), before);
  CHECK_EQ(countAssets(p), std::size_t{0});
}
TEST(Atomicity, FailedMoveLeavesHierarchyIntact) {
  Project p = Project::create("demo");
  GroupNodeData g1{"g1", Transform(), true};
  const NodeId a = p.createGroupNode(p.rootNodeId(), g1);
  GroupNodeData g2{"g2", Transform(), true};
  const NodeId b = p.createGroupNode(a, g2);
  const std::size_t kidsA = p.childrenOf(a).size();
  CHECK(!p.moveNode(a, b, 0).isOk());
  CHECK_EQ(p.childrenOf(a).size(), kidsA);
  CHECK_EQ(p.parentOf(a)->common.id, p.rootNodeId());
  CHECK(p.validate().empty());
}
TEST(Atomicity, FailedReorderLeavesOrderIntact) {
  Project p = Project::create("demo");
  GroupNodeData g{"g", Transform(), true};
  const NodeId a = p.createGroupNode(p.rootNodeId(), g);
  GroupNodeData h{"h", Transform(), true};
  const NodeId b = p.createGroupNode(p.rootNodeId(), h);
  CHECK(!p.reorderChild(p.rootNodeId(), 0, 99).isOk());
  const std::vector<const NodeRecord*> kids = p.childrenOf(p.rootNodeId());
  CHECK_EQ(kids.size(), std::size_t{2});
  CHECK_EQ(kids[0]->common.id, a);
  CHECK_EQ(kids[1]->common.id, b);
}
TEST(Atomicity, FailedTransformKeepsOldValue) {
  Project p = Project::create("demo");
  GroupNodeData g{"g", Transform(), true};
  const NodeId n = p.createGroupNode(p.rootNodeId(), g);
  Transform bad;
  bad.x = std::numeric_limits<double>::quiet_NaN();
  CHECK(!p.setNodeTransform(n, bad).isOk());
  CHECK_EQ(p.findNode(n)->common.transform.x, 0.0);
}
TEST(Atomicity, FailedDeleteKeepsSubtree) {
  Project p = Project::create("demo");
  GroupNodeData g{"g", Transform(), true};
  const NodeId a = p.createGroupNode(p.rootNodeId(), g);
  GroupNodeData c{"c", Transform(), true};
  const NodeId b = p.createGroupNode(a, c);
  CHECK(!p.deleteNode(a, ChildPolicy::kRejectIfHasChildren).isOk());
  CHECK(p.findNode(a) != nullptr);
  CHECK(p.findNode(b) != nullptr);
  CHECK(p.validate().empty());
}
