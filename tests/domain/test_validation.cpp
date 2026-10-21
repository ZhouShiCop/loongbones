// 校验用例：用 adoptData 装载**非法数据**，逐条验证规则能被查出。

#include "test_framework.hpp"

#include <cmath>
#include <limits>
#include <vector>

#include "loongbones/domain/project.hpp"

using namespace loongbones;

namespace {
bool hasCode(const std::vector<Diagnostic>& ds, DiagnosticCode c) {
  for (const Diagnostic& d : ds) if (d.code == c) return true;
  return false;
}
ProjectData baseData() {
  Project p = Project::create("demo");
  GroupNodeData g{"g", Transform(), true};
  p.createGroupNode(p.rootNodeId(), g);
  return p.data();
}
}  // namespace
TEST(Validation, CleanDataPasses) {
  Project p;
  CHECK(p.adoptData(baseData()).isOk());
  CHECK(p.validate().empty());
}

TEST(Validation, DuplicateNodeIdDetected) {
  // 构造 3 节点：根 + g1 + g2，再把 g2 的 id 改成 g1 的 id 制造重复
  Project p = Project::create("demo");
  GroupNodeData gd{"g1", Transform(), true};
  const NodeId g1 = p.createGroupNode(p.rootNodeId(), gd);
  gd.name = "g2";
  const NodeId g2 = p.createGroupNode(p.rootNodeId(), gd);
  ProjectData d = p.data();
  for (NodeRecord& n : d.nodes) {
    if (n.common.id == g2) n.common.id = g1;  // g2 顶替 g1 的 id
  }
  Project p2;
  CHECK(!p2.adoptData(d).isOk());
  CHECK_EQ(p2.lastStatus().code, DiagnosticCode::kIdDuplicate);
}
TEST(Validation, MalformedIdDetected) {
  ProjectData d = baseData();
  const NodeId bad = Id::fromString(IdKind::Node, "BAD");
  d.nodes[1].common.id = bad;
  d.nodeOrder[1] = bad;
  Project p;
  CHECK(!p.adoptData(d).isOk());
  CHECK_EQ(p.lastStatus().code, DiagnosticCode::kIdMalformed);
}
TEST(Validation, NonFiniteTransformDetected) {
  ProjectData d = baseData();
  d.nodes[1].common.transform.x = std::numeric_limits<double>::quiet_NaN();
  Project p;
  CHECK(!p.adoptData(d).isOk());
  CHECK(hasCode(p.validate(), DiagnosticCode::kTransformNotFinite) ||
        p.lastStatus().code == DiagnosticCode::kTransformNotFinite);
}

TEST(Validation, InfiniteTransformDetected) {
  ProjectData d = baseData();
  d.nodes[1].common.transform.scaleY = std::numeric_limits<double>::infinity();
  Project p;
  CHECK(!p.adoptData(d).isOk());
}
TEST(Validation, TypeDataMismatchDetected) {
  ProjectData d = baseData();
  d.nodes[1].type = NodeType::kImage;
  Project p;
  CHECK(!p.adoptData(d).isOk());
}

TEST(Validation, GroupHoldingAssetDetected) {
  ProjectData d = baseData();
  d.nodes[1].type = NodeType::kGroup;
  d.nodes[1].assetId = Id::generate(IdKind::ImageAsset);
  Project p;
  CHECK(!p.adoptData(d).isOk());
}
TEST(Validation, RootMismatchDetected) {
  ProjectData d = baseData();
  // 兼容字段 rootNodeId 与场景根不一致 -> 拒绝（单场景契约）
  d.rootNodeId = Id::generate(IdKind::Node);
  Project p;
  CHECK(!p.adoptData(d).isOk());
  CHECK(p.lastStatus().code == DiagnosticCode::kSceneRootInvalid);
}

TEST(Validation, CycleDetected) {
  ProjectData d = baseData();
  d.nodes[0].common.parent = d.nodes[1].common.id;
  Project p;
  CHECK(!p.adoptData(d).isOk());
}

TEST(Validation, OrphanNodeDetected) {
  ProjectData d = baseData();
  d.nodes[1].common.parent = Id::generate(IdKind::Node);
  d.nodes[0].common.children.clear();
  Project p;
  CHECK(!p.adoptData(d).isOk());
}