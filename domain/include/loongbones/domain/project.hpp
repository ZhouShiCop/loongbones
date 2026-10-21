// Project: 工程数据容器与唯一写入口（Task1 3.1 / 5）
// 只管内存业务数据: 无 JSON 序列化、无 GUI、无图片解码、无文件系统。
// 外部只能拿到 const 视图; 所有修改经本类方法, 无法绕过数据约束。
// 每个写操作先完整校验再一次性提交; 失败不留半完成状态。

#ifndef LOONGBONES_DOMAIN_PROJECT_HPP
#define LOONGBONES_DOMAIN_PROJECT_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "loongbones/domain/atlas.hpp"
#include "loongbones/domain/diagnostics.hpp"
#include "loongbones/domain/ids.hpp"
#include "loongbones/domain/image_asset.hpp"
#include "loongbones/domain/node.hpp"
#include "loongbones/domain/skeleton.hpp"
#include "loongbones/domain/transform.hpp"

namespace loongbones {
/// 删除节点时对子节点的处理策略（Task1 5: 删除分组不得隐式丢失子树,
/// 存在歧义时须由调用方显式指定策略）。
enum class ChildPolicy {
  kRejectIfHasChildren,
  kDeleteSubtree,
};

/// 场景：一棵节点树的容器，持有唯一根节点。Project 与 Scene 不混为同一概念。
///
/// 本 Task 只稳定支持"至少一个默认 Scene"；多 Scene 的公开 API 留给后续 RFC，
/// 不引入隐式全局状态。
struct Scene {
  SceneId id;
  std::string name;
  /// 该场景唯一的树根节点。根无父节点、恒等局部变换、不绘制。
  NodeId rootNodeId;
};

/// 工程数据。成员均为显式值类型, 无 Qt、无文件句柄、无 GUI 状态。
struct ProjectData {
  ProjectId id;
  std::string name;
  /// Task 1 兼容字段：默认场景的根。与 scenes[defaultSceneIndex].rootNodeId 同源。
  NodeId rootNodeId;
  /// 场景集合；本 Task 至少含一个默认场景
  std::vector<Scene> scenes;
  /// 默认场景在 scenes 中的下标（不依赖哈希容器迭代次序）
  std::size_t defaultSceneIndex = 0;
  /// 节点记录; 与 nodeOrder 平行同序, 决定遍历顺序（确定性）
  std::vector<NodeRecord> nodes;
  std::vector<NodeId> nodeOrder;
  std::vector<ImageAsset> assets;
  /// 图集资源层（Task 2）
  std::vector<TextureAsset> textures;
  std::vector<Atlas> atlases;
  /// 骨架层（Task 4 B2）。Skeleton 是 Project 级资产；Skin 只嵌套于 Skeleton，
  /// 工程顶层没有 skins[]（D27 / v2 schema §1）。
  std::vector<Skeleton> skeletons;
};
class Project {
 public:
  /// 新建工程: 生成 ID 并创建空 GroupNode "root" 作为根
  static Project create(std::string projectName);
  /// 以既有 ID 新建（导入/反序列化用）
  static Project createWithId(ProjectId id, std::string projectName);
  Project() = default;

  // ---- 只读访问（外部不得直接修改集合）----
  const ProjectData& data() const noexcept { return data_; }
  const ProjectId& id() const noexcept { return data_.id; }
  const std::string& name() const noexcept { return data_.name; }
  const NodeId& rootNodeId() const noexcept { return data_.rootNodeId; }
  /// 全部节点的确定顺序视图（深度优先, 子按 children 顺序）
  std::vector<const NodeRecord*> nodesInOrder() const;
  const NodeRecord* findNode(const NodeId& id) const;
  const ImageAsset* findAsset(const ImageAssetId& id) const;
  const NodeRecord* parentOf(const NodeId& id) const;
  std::vector<const NodeRecord*> childrenOf(const NodeId& id) const;
  std::vector<const NodeRecord*> nodesReferencing(const ImageAssetId& id) const;
  bool isDescendantOf(const NodeId& node, const NodeId& ancestor) const;
  // ---- 写操作（统一入口）----

  /// 创建图片资源记录。名字在本工程内必须唯一。
  ImageAssetId createImageAsset(std::string assetName, StorageMode storage, std::string location);
  ImageAssetId createImageAsset(std::string assetName);

  /// 在 parent 下创建分组节点
  NodeId createGroupNode(const NodeId& parent, GroupNodeData data);
  /// 在 parent 下创建图片节点。asset 必须已存在（校验规则 6）
  NodeId createImageNode(const NodeId& parent, ImageNodeData data);
  /// 在 parent 下创建 Sprite 节点，引用一个已存在的图集区域（Task 2 Back Task 2.3）。
  /// 仅 kImage 角色可引用区域；region 必须存在。
  NodeId createSpriteNode(const NodeId& parent, SpriteNodeData data);
  /// 改变某节点的图集区域引用（仅对图片节点有意义）。切换为区域引用时清除 assetId。
  Status setNodeRegion(const NodeId& id, const AtlasRegionId& regionId);

  Status renameNode(const NodeId& id, std::string newName);
  Status setNodeTransform(const NodeId& id, const Transform& transform);
  Status setNodeVisible(const NodeId& id, bool visible);
  Status renameProject(std::string newName);
  /// 设置节点锁定状态（仅编辑期语义，不影响几何/绘制）
  Status setNodeLocked(const NodeId& id, bool locked);

  // ---- 场景（Task 2）----

  /// 默认场景；scenes 为空时返回 nullptr
  [[nodiscard]] const Scene* defaultScene() const;
  [[nodiscard]] const Scene* findScene(const SceneId& id) const;
  /// 设置默认场景下标；越界返回失败
  Status setDefaultScene(std::size_t index);

  // ---- 图集资源层（Task 2）----

  TextureAssetId createTextureAsset(std::string name, StorageMode storage, std::string location);
  AtlasId createAtlas(std::string name);
  /// 在图集内新建页；page.textureAssetId 必须已存在
  Status addAtlasPage(const AtlasId& atlas, const AtlasPage& page);
  /// 在图集内新建区域；pageId 必须属于该图集且元数据合法
  Status addAtlasRegion(const AtlasId& atlas, const AtlasRegion& region);
  /// 把 ImageAsset 适配为"整图区域"（Task 1 兼容映射，不重建 ID）
  AtlasRegionId adoptImageAssetAsWholeImageRegion(const ImageAssetId& assetId);

  [[nodiscard]] const AtlasRegion* findRegion(const AtlasRegionId& id) const;
  [[nodiscard]] const AtlasPage* findPage(const AtlasPageId& id) const;
  [[nodiscard]] const TextureAsset* findTextureAsset(const TextureAssetId& id) const;
  [[nodiscard]] std::vector<const NodeRecord*> nodesReferencingRegion(const AtlasRegionId& id) const;
  /// 删除区域；仍被节点引用则失败
  Status deleteAtlasRegion(const AtlasRegionId& id);

  // ---- 绘制顺序（Task 2 4.2.5）----

  /// 确定性遍历：先自身，再按 children 顺序递归。含场景根（根不绘制，
  /// 但作为树锚点被纳入，便于导出/迭代时保持完整树形）。
  /// 若只需可绘制节点，见 drawableOrder()。
  [[nodiscard]] std::vector<const NodeRecord*> drawOrder() const;
  /// 渲染节点顺序：drawOrder() 去掉场景根之后的结果（根不参与绘制）。
  [[nodiscard]] std::vector<const NodeRecord*> drawableOrder() const;

  /// 把节点移到 newParent 下的第 index 个位置（省略则追加到末尾）
  Status moveNode(const NodeId& id, const NodeId& newParent, std::size_t index);
  /// 调整同一分组内的节点顺序
  Status reorderChild(const NodeId& parent, std::size_t fromIndex, std::size_t toIndex);
  /// 删除节点。策略必须显式给出, 不隐式丢弃子树。
  Status deleteNode(const NodeId& id, ChildPolicy policy);
  /// 删除未被引用的图片资源（校验规则 8）
  Status deleteImageAsset(const ImageAssetId& id);
  // ---- 一致性校验 ----

  /// 全量校验, 返回全部违规诊断（空表示通过）
  std::vector<Diagnostic> validate() const;
  /// 上一个失败操作的诊断; 成功时 code == kNone
  const Diagnostic& lastStatus() const noexcept { return lastStatus_; }
  /// 供导入/反序列化装载数据; 整体校验不通过则拒绝
  Status adoptData(ProjectData data);
  Status adoptRoot(const NodeId& groupNodeId);
 private:
  ProjectData data_{};
  Diagnostic lastStatus_{};

  static constexpr std::size_t kNpos = static_cast<std::size_t>(-1);
  std::size_t indexOfNode(const NodeId& id) const;
  std::size_t indexOfAsset(const ImageAssetId& id) const;
  std::size_t indexOfAtlas(const AtlasId& id) const;
  void detachChild(NodeCommon& parent, const NodeId& child);
  Status recordFailure(DiagnosticCode code, std::string message, std::string location = {});
  Status succeed() { lastStatus_ = Diagnostic(); return Status::ok(); }
};

/// 深度优先遍历（父在前, 子按 children 顺序）。环状数据由 visited 集合兜底,
/// 但环本身须由 validate() 报告。
std::vector<const NodeRecord*> traverseDepthFirst(const ProjectData& data);

}  // namespace loongbones

#endif  // LOONGBONES_DOMAIN_PROJECT_HPP