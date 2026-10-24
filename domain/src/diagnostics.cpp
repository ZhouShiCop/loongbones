// 诊断码与操作结果。诊断码字符串是稳定契约（规范 10.3）。

#include "loongbones/domain/diagnostics.hpp"

namespace loongbones {

const char* toString(DiagnosticCode code) noexcept {
  switch (code) {
    case DiagnosticCode::kNone: return "NONE";
    case DiagnosticCode::kIdMalformed: return "ID_MALFORMED";
    case DiagnosticCode::kIdDuplicate: return "ID_DUPLICATE";
    case DiagnosticCode::kIdKindMismatch: return "ID_KIND_MISMATCH";
    case DiagnosticCode::kNodeNotFound: return "NODE_NOT_FOUND";
    case DiagnosticCode::kNodeTypeMismatch: return "NODE_TYPE_MISMATCH";
    case DiagnosticCode::kRootMissing: return "ROOT_MISSING";
    case DiagnosticCode::kRootNotGroup: return "ROOT_NOT_GROUP";    case DiagnosticCode::kRootParented: return "ROOT_PARENTED";
    case DiagnosticCode::kNodeHasParent: return "NODE_HAS_PARENT";
    case DiagnosticCode::kNodeOrphan: return "NODE_ORPHAN";
    case DiagnosticCode::kParentChildMismatch: return "PARENT_CHILD_MISMATCH";
    case DiagnosticCode::kCycleDetected: return "CYCLE_DETECTED";
    case DiagnosticCode::kNodeHasChildren: return "NODE_HAS_CHILDREN";
    case DiagnosticCode::kCannotMoveIntoSelf: return "CANNOT_MOVE_INTO_SELF";
    case DiagnosticCode::kGroupCannotHoldAsset: return "GROUP_CANNOT_HOLD_ASSET";
    case DiagnosticCode::kImageCannotHoldChildren:
      return "IMAGE_CANNOT_HOLD_CHILDREN";  // deprecated: 见 diagnostics.hpp
    case DiagnosticCode::kSceneNotFound: return "SCENE_NOT_FOUND";
    case DiagnosticCode::kSceneDuplicate: return "SCENE_DUPLICATE";
    case DiagnosticCode::kRootImmutable: return "ROOT_IMMUTABLE";
    case DiagnosticCode::kDuplicateChild: return "DUPLICATE_CHILD";
    case DiagnosticCode::kInvalidRegionRect: return "INVALID_REGION_RECT";
    case DiagnosticCode::kInvalidRotation: return "INVALID_ROTATION";
    case DiagnosticCode::kRegionNotFound: return "REGION_NOT_FOUND";
    case DiagnosticCode::kPageNotFound: return "PAGE_NOT_FOUND";
    case DiagnosticCode::kTextureAssetNotFound: return "TEXTURE_ASSET_NOT_FOUND";
    case DiagnosticCode::kRegionInUse: return "REGION_IN_USE";
    case DiagnosticCode::kPageInUse: return "PAGE_IN_USE";
    case DiagnosticCode::kTextureInUse: return "TEXTURE_IN_USE";
    case DiagnosticCode::kAssetNotFound: return "ASSET_NOT_FOUND";
    case DiagnosticCode::kAssetNameDuplicate: return "ASSET_NAME_DUPLICATE";
    case DiagnosticCode::kAssetInUse: return "ASSET_IN_USE";
    case DiagnosticCode::kTransformNotFinite: return "TRANSFORM_NOT_FINITE";
    case DiagnosticCode::kProjectInvalid: return "PROJECT_INVALID";
    case DiagnosticCode::kSceneCountMismatch: return "SCENE_COUNT_MISMATCH";
    case DiagnosticCode::kSceneRootInvalid: return "SCENE_ROOT_INVALID";
    case DiagnosticCode::kRootNotUnique: return "ROOT_NOT_UNIQUE";
    case DiagnosticCode::kPageNotInAtlas: return "PAGE_NOT_IN_ATLAS";
    case DiagnosticCode::kInvalidTrim: return "INVALID_TRIM";
    case DiagnosticCode::kSkeletonEmpty: return "SKELETON_EMPTY";
    case DiagnosticCode::kSkeletonRootBoneMissing: return "SKELETON_ROOT_BONE_MISSING";
    case DiagnosticCode::kSkeletonBoneParentNotFound: return "SKELETON_BONE_PARENT_NOT_FOUND";
    case DiagnosticCode::kSkeletonBoneParentForwardRef: return "SKELETON_BONE_PARENT_FORWARD_REF";
    case DiagnosticCode::kSkeletonBoneOrderNotPermutation:
      return "SKELETON_BONE_ORDER_NOT_PERMUTATION";
    case DiagnosticCode::kSkeletonSlotBoneNotFound: return "SKELETON_SLOT_BONE_NOT_FOUND";
    case DiagnosticCode::kSkeletonDrawOrderNotPermutation:
      return "SKELETON_DRAW_ORDER_NOT_PERMUTATION";
    case DiagnosticCode::kSkeletonSkinSlotNotFound: return "SKELETON_SKIN_SLOT_NOT_FOUND";
    case DiagnosticCode::kSkeletonSkinEntryDuplicate: return "SKELETON_SKIN_ENTRY_DUPLICATE";
    case DiagnosticCode::kSkeletonSkinMissingDefault: return "SKELETON_SKIN_MISSING_DEFAULT";
    case DiagnosticCode::kSkeletonAtlasNotFound: return "SKELETON_ATLAS_NOT_FOUND";
    case DiagnosticCode::kSkeletonValueNotFinite: return "SKELETON_VALUE_NOT_FINITE";
    case DiagnosticCode::kSkeletonSourceRefInvalid: return "SKELETON_SOURCE_REF_INVALID";

    // ---- 骨架第二批（Task 5 B1）----
    case DiagnosticCode::kMeshTriangleIndexOutOfRange: return "MESH_TRIANGLE_INDEX_OUT_OF_RANGE";
    case DiagnosticCode::kMeshVertexGeometryInvalid: return "MESH_VERTEX_GEOMETRY_INVALID";
    case DiagnosticCode::kMeshUvOutOfRange: return "MESH_UV_OUT_OF_RANGE";
    case DiagnosticCode::kMeshLinkedParentUnresolved: return "MESH_LINKED_PARENT_UNRESOLVED";
    case DiagnosticCode::kMeshWeightSumInvalid: return "MESH_WEIGHT_SUM_INVALID";
    case DiagnosticCode::kMeshWeightBoneUnresolved: return "MESH_WEIGHT_BONE_UNRESOLVED";
    case DiagnosticCode::kMeshBindingCountInvalid: return "MESH_BINDING_COUNT_INVALID";
    case DiagnosticCode::kConstraintTargetUnresolved: return "CONSTRAINT_TARGET_UNRESOLVED";
    case DiagnosticCode::kConstraintChainEmpty: return "CONSTRAINT_CHAIN_EMPTY";
  }
  return "UNKNOWN";
}

std::string Diagnostic::str() const {
  std::string s = toString(code);
  if (!location.empty()) { s += " @ "; s += location; }
  if (!message.empty()) { s += ": "; s += message; }
  return s;
}

}  // namespace loongbones