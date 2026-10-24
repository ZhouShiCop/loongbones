// 诊断与操作结果——规范 §8.3（结构化诊断）、§10.3（诊断 code 稳定大写域前缀）
//
// 设计要点：
//   · 错误不用异常表达，而是返回值。理由：Task 1 要求「操作失败时不得留下半完成状态」，
//     返回值形式让每个写操作的「先校验、后提交」结构在代码上显而易见。
//   · 每条诊断都带 location（可定位），满足 Task 1 §4「校验失败时应提供可定位的错误信息」。
//   · 程序逻辑不得按文案判断（规范 §10.3），因此 code 是稳定枚举，文案仅供人读。

#ifndef LOONGBONES_DOMAIN_DIAGNOSTICS_HPP
#define LOONGBONES_DOMAIN_DIAGNOSTICS_HPP

#include <string>
#include <utility>

namespace loongbones {

/// 稳定诊断码。新增只能追加，不得复用或改写既有含义。
enum class DiagnosticCode {
  kNone = 0,

  // ---- 标识 ----
  kIdMalformed,        ///< 标识格式非法
  kIdDuplicate,        ///< 标识重复
  kIdKindMismatch,     ///< 标识种类不符

  // ---- 节点与层级 ----
  kNodeNotFound,             ///< 节点不存在
  kNodeTypeMismatch,         ///< 节点类型与实际数据结构不一致（校验规则 9）
  kRootMissing,              ///< 根节点不存在（校验规则 2）
  kRootNotGroup,             ///< 根节点类型不是 GroupNode（校验规则 2）
  kRootParented,             ///< 根节点被当作普通子节点挂载（Task 1 §3.2）
  kNodeHasParent,            ///< 节点已属于某个父节点（校验规则 3）
  kNodeOrphan,               ///< 非根节点没有父节点（校验规则 3）
  kParentChildMismatch,      ///< 父子关系双向不一致（校验规则 4）
  kCycleDetected,            ///< 存在循环层级（校验规则 5）
  kNodeHasChildren,          ///< 删除时仍有子节点，需显式策略
  kCannotMoveIntoSelf,       ///< 不能把节点移入自身或其子树
  kGroupCannotHoldAsset,     ///< GroupNode 不允许直接引用 ImageAsset（校验规则 7）

  /// @deprecated 已废止的限制。Task 2 §4.2.2 移除「图片节点不能持有子节点」后，
  /// 任何普通节点都可挂载，本码**不再用于**合法的父子挂载失败。
  /// 保留枚举位置与字符串仅为兼容既有诊断契约，不得复用、不得改含义。
  kImageCannotHoldChildren,

  // ---- 场景 / 根（Task 2）----
  kSceneNotFound,        ///< 场景不存在
  kSceneDuplicate,       ///< 场景 ID 或名称重复
  kRootImmutable,        ///< 根的恒等变换/身份被试图修改
  kDuplicateChild,       ///< 同一父节点的 children 中出现重复子引用
  kInvalidRegionRect,    ///< 图集区域矩形非法（非正或越界）
  kInvalidRotation,      ///< 区域旋转枚举值非法
  kRegionNotFound,       ///< 引用的 AtlasRegion 不存在
  kPageNotFound,         ///< 引用的 AtlasPage 不存在
  kTextureAssetNotFound, ///< 引用的 TextureAsset 不存在
  kRegionInUse,          ///< 区域仍被节点引用，不允许删除
  kPageInUse,            ///< 页仍被区域引用，不允许删除
  kTextureInUse,         ///< 纹理资产仍被页引用，不允许删除

  // ---- 资源 ----
  kAssetNotFound,       ///< 引用的 ImageAsset 不存在（校验规则 6）
  kAssetNameDuplicate,  ///< 资源重名
  kAssetInUse,          ///< 资源仍被节点引用，不允许删除（校验规则 8）

  // ---- 变换 ----
  kTransformNotFinite,  ///< 变换字段出现 NaN / Inf（校验规则 10）

  // ---- 工程 ----
  kProjectInvalid,  ///< 工程整体不一致

  // ---- Task 2 Back Task 新增（不得复用、不得改含义）----
  kSceneCountMismatch,   ///< 场景数量不符合单场景契约（仅支持一个 Scene）
  kSceneRootInvalid,     ///< 场景根指向缺失或非 kRoot 节点
  kRootNotUnique,        ///< 工程中存在多个根节点
  kPageNotInAtlas,       ///< 区域引用的页不属于同一图集
  kInvalidTrim,          ///< trim 偏移/尺寸非法（非负且落在原始尺寸内）

  // ---- 骨架（Task 4 B2）----
  // 契约：docs/format/native-project-v2-skeleton.md §9。只追加，不复用既有含义。
  kSkeletonEmpty,               ///< 骨架没有任何骨骼
  kSkeletonRootBoneMissing,     ///< 骨架没有根骨骼（parentId 为空的骨骼）
  kSkeletonBoneParentNotFound,  ///< 骨骼 parentId 指向同一骨架内不存在的骨骼
  kSkeletonBoneParentForwardRef, ///< 骨骼 parentId 前向引用（指向排序在后的骨骼）
  kSkeletonBoneOrderNotPermutation,  ///< boneOrder 与 bones[].id 不互为排列
  kSkeletonSlotBoneNotFound,    ///< 插槽 boneId 指向不存在的骨骼
  kSkeletonDrawOrderNotPermutation,  ///< drawOrder 与 slots[].id 不互为排列
  kSkeletonSkinSlotNotFound,    ///< 皮肤条目 slotId 指向不存在的插槽
  kSkeletonSkinEntryDuplicate,  ///< 同一皮肤内 (slotId, attachmentName) 重复
  kSkeletonSkinMissingDefault,  ///< 骨架缺少 default 皮肤
  kSkeletonAtlasNotFound,       ///< skeleton.atlasIds 引用不存在的图集
  kSkeletonValueNotFinite,      ///< 骨架数值字段出现 NaN / Inf
  kSkeletonSourceRefInvalid,    ///< sourceRef 存在但必需字段为空或版本号为负

  // ---- 骨架第二批（Task 5 B1）----
  // 契约：docs/skeleton/mesh-and-constraint-rfc.md §11。只追加，不复用既有含义。
  kMeshTriangleIndexOutOfRange,  ///< mesh/加权 mesh 的三角索引 ≥ 顶点数
  kMeshVertexGeometryInvalid,    ///< 顶点数为 0，或 uvs / vertices 长度不是偶数
  kMeshUvOutOfRange,             ///< mesh UV 落在 [0,1] 之外
  kMeshLinkedParentUnresolved,   ///< linked mesh 的 parentKey 解析不到父附件
  kMeshWeightSumInvalid,         ///< 某顶点权重和为 0、< 0 或非有限（D37）
  kMeshWeightBoneUnresolved,     ///< 绑定的 boneId 指向不存在的骨骼，或源下标非整数
  kMeshBindingCountInvalid,      ///< 某顶点的绑定列表为空
  kConstraintTargetUnresolved,   ///< IK / transform 的骨骼或目标骨指向不存在的骨骼
  kConstraintChainEmpty,         ///< IK 的骨骼链为空
};

/// 诊断码的稳定字符串形式。程序逻辑请用枚举，不要用这个字符串。
const char* toString(DiagnosticCode code) noexcept;

/// 一条诊断。
struct Diagnostic {
  DiagnosticCode code = DiagnosticCode::kNone;
  /// 面向人的可读说明（可本地化；不得用于程序分支）
  std::string message;
  /// 可定位信息，例如 "nodes/01J8Z..."、"assets"、""（工程级）
  std::string location;

  Diagnostic() = default;
  Diagnostic(DiagnosticCode c, std::string msg, std::string loc = {})
      : code(c), message(std::move(msg)), location(std::move(loc)) {}

  [[nodiscard]] std::string str() const;
};

/// 操作结果：成功，或带一条诊断的失败。
class Status {
 public:
  Status() = default;

  static Status ok() { return Status(); }

  static Status fail(DiagnosticCode code, std::string message, std::string location = {}) {
    Status s;
    s.ok_ = false;
    s.diagnostic_ = Diagnostic(code, std::move(message), std::move(location));
    return s;
  }

  [[nodiscard]] bool isOk() const noexcept { return ok_; }
  [[nodiscard]] const Diagnostic& diagnostic() const noexcept { return diagnostic_; }

  explicit operator bool() const noexcept { return ok_; }

 private:
  bool ok_ = true;
  Diagnostic diagnostic_;
};

}  // namespace loongbones

#endif  // LOONGBONES_DOMAIN_DIAGNOSTICS_HPP