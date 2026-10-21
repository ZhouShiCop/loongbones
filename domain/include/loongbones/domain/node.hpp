// 节点数据模型（Task1 3.2-3.4）
// 不含 JSON 序列化、GUI、图片解码与渲染。

#ifndef LOONGBONES_DOMAIN_NODE_HPP
#define LOONGBONES_DOMAIN_NODE_HPP

#include <optional>
#include <string>
#include <vector>

#include "loongbones/domain/ids.hpp"
#include "loongbones/domain/image_asset.hpp"
#include "loongbones/domain/transform.hpp"

namespace loongbones {

/// 节点种类。Task 2 层级不变量：**除 kRoot 外没有挂载特权**。
/// kGroup 与 kImage 是平权的普通节点，都可作为父节点和子节点；
/// 只有 kRoot 是唯一特殊身份（无父、恒等变换、不绘制、不可移动或删除）。
enum class NodeType {
  kRoot,   ///< 场景树唯一根
  kGroup,  ///< 无自身绘制内容的组织节点
  kImage,  ///< 有自身图片内容引用的节点（Task 2 语义中的 SpriteNode 角色）
};

inline bool isRootType(NodeType t) noexcept { return t == NodeType::kRoot; }
inline bool isOrdinaryNode(NodeType t) noexcept { return t != NodeType::kRoot; }

struct NodeCommon {
  NodeId id;
  std::string name;
  /// 父节点 ID；场景根为空（默认构造的 Id 即无）
  NodeId parent;
  /// 子节点 ID 的有序列表。顺序即语义：定义该节点下的绘制/遍历顺序。
  std::vector<NodeId> children;
  /// 局部变换（Y-up，度；见 ADR-0003 C1/C2）
  Transform transform;
  bool visible = true;
  /// 编辑期锁定。默认 false；仅表达不可编辑，不影响几何/绘制语义。
  bool locked = false;
};

/// 节点记录。用「公共数据 + 类型标签 + 可选资源引用」表达,
/// 使校验规则 9「节点类型与实际数据结构一致」成为可检查的真实约束:
/// kGroup 时 assetId 必须缺省（同时满足校验规则 7）, kImage 时必须存在。
struct NodeRecord {
  NodeCommon common;
  NodeType type = NodeType::kGroup;
  /// 图片内容引用（Task 1 语义，保留以维持 ID/API 兼容）
  std::optional<ImageAssetId> assetId;
  /// 可选：引用的图集区域（Task 2 引入；节点只存稳定 ID，不复制区域几何）
  std::optional<AtlasRegionId> regionId;

  /// 类型与数据结构是否自洽（校验规则 9 的判据）
  ///   · kImage 必须且只能引用「一个」内容源：要么 ImageAsset（Task 1 兼容），
  ///     要么 AtlasRegion（Task 2 Sprite 语义）。二者互斥，不得同时或同时不持。
  ///   · kRoot 与 kGroup 既不持有 ImageAsset 也不持有 AtlasRegion。
  bool typeMatchesData() const noexcept {
    if (type == NodeType::kImage) {
      return assetId.has_value() != regionId.has_value();  // 恰好其一
    }
    return !assetId.has_value() && !regionId.has_value();
  }
};
struct GroupNodeData {
  std::string name;
  Transform transform;
  bool visible = true;
};

struct ImageNodeData {
  std::string name;
  ImageAssetId assetId;
  Transform transform;
  bool visible = true;
};

/// 创建引用图集区域的 Sprite 节点所需数据（Task 2 Back Task 2.3）。
/// 与 ImageNodeData 互斥：一个图片节点要么引用 ImageAsset，要么引用 AtlasRegion。
struct SpriteNodeData {
  std::string name;
  AtlasRegionId regionId;
  Transform transform;
  bool visible = true;
};

}  // namespace loongbones

#endif  // LOONGBONES_DOMAIN_NODE_HPP