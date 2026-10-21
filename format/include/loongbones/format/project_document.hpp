// 格式层文档对象（RFC §6.5）
//
// **未知字段由 Format 层的本结构持有，ProjectData（Domain）不新增任何扩展数据成员。**
// 理由：规范禁止 Domain 依赖 JSON 库 / 格式版本细节；把「未识别的 JSON 子树」
// 放进 ProjectData 会让 Domain 承载纯格式概念，违反 Format -> Domain 单向依赖。

#ifndef LOONGBONES_FORMAT_PROJECT_DOCUMENT_HPP
#define LOONGBONES_FORMAT_PROJECT_DOCUMENT_HPP

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "loongbones/domain/project.hpp"
#include "loongbones/format/json_value.hpp"

namespace loongbones {
namespace format {

/// 未知字段宿主的对象种类（RFC §6.5.1）。
///
/// 必须与 id **组合**使用：Domain 只保证 ID 在**同类内**唯一
/// （kIdDuplicate 覆盖 node/asset/texture/atlas/page/region，kSceneDuplicate 覆盖 scene），
/// 而各类 ID 的字符串格式完全相同（都是 26 字符 ULID），故单用 id 无法区分种类。
enum class OwnerKind {
  kDocument,  ///< 顶层文档对象（唯一，无 ID）
  kProject,
  kScene,
  kNode,
  kImageAsset,
  kTextureAsset,
  kAtlas,
  kAtlasPage,
  kAtlasRegion,

  // ---- 骨架层（Task 4 B2/B3，v2 schema §5.1）----
  // 只追加，不得复用或改写既有取值含义。
  kSkeleton,   ///< Skeleton（稳定 ID）
  kBone,       ///< Bone（稳定 ID）
  kSlot,       ///< Slot（稳定 ID）
  kSkin,       ///< Skin（稳定 ID）
  kSkinEntry,  ///< SkinEntry（稳定 ID）
                ///  Attachment **没有**独立 OwnerKind：与 SkinEntry 1:1 内嵌，
                ///  锚点 = (kSkinEntry, entryId) + "/attachment"（v2 schema §5.2）。
};

/// 宿主锚点：kind + 稳定 ID（**位置无关**）。
/// 用位置（JSON Pointer）作锚点会在数组重排后把字段挂到错误的宿主上（RFC §6.5.1 反例）。
struct OwnerAnchor {
  OwnerKind kind = OwnerKind::kDocument;
  std::string id;

  friend bool operator<(const OwnerAnchor& a, const OwnerAnchor& b) noexcept {
    if (a.kind != b.kind) return a.kind < b.kind;
    return a.id < b.id;
  }
};

/// 一个容器上的未知键集合，按**首次出现顺序**保存（RFC §6.2）
struct UnknownBucket {
  std::vector<std::pair<std::string, JsonValue>> entries;
};

/// 索引键 = (锚点, 宿主内相对子路径)。**不含任何位置信息**。
/// subPath 形如 ""（宿主对象本身）或 "/transform"（无 ID 的嵌套容器）。
using UnknownKey = std::pair<OwnerAnchor, std::string>;
using UnknownFieldIndex = std::map<UnknownKey, UnknownBucket>;

/// 格式层文档对象 = 已校验的 ProjectData + 旁路保留的未知字段 + 能力声明。
///
/// capabilities 是**已知**的顶层字段，故不入 unknowns；其内容需逐项保留：
/// required 逐项必须是已知能力（否则拒绝），optional 的不认识条目按 RFC §1.2
/// 「可安全忽略，但内容仍须保留」，因此原样存下并在编码时原样写出。
/// 文档的来源格式版本（Task 4 B3）。
///
/// 注意这是「**读进来的那个文件的版本**」，不是「将要写出的版本」：
/// 写出**永远**是 v2（B3 第 5 条），因此本字段只用于判断"是否已由 v1 迁入"，
/// 以及在保存前告知用户"保存会把工程升级到 v2"。
enum class SourceFormatVersion {
  kUnknown = 0,
  kV1 = 1,  ///< 磁盘上是 v1；内存中已被迁移为 v2 结构
  kV2 = 2,
};

struct ProjectDocument {
  ProjectData data;
  UnknownFieldIndex unknowns;

  bool hasCapabilities = false;
  std::vector<std::string> requiredCapabilities;
  std::vector<std::string> optionalCapabilities;

  /// 读入时的文档版本（B3 第 1 条：文档版本状态）。
  /// 默认 kV2 —— 新建文档（createProjectDocument）直接是 v2。
  SourceFormatVersion sourceVersion = SourceFormatVersion::kV2;
};

}  // namespace format
}  // namespace loongbones

#endif  // LOONGBONES_FORMAT_PROJECT_DOCUMENT_HPP
