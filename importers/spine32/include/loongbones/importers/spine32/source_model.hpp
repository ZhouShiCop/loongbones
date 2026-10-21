// Spine 3.2.01 源格式私有 IR（Task 4 B4）
//
// 纪律（Task4_Implementation_Plan.md §1 / §6）：
//   · 本 IR **只存在于 importers/spine32/**；Domain、Format、宿主公共 API 不得看见它。
//   · 它是"源格式对象"，不是 canonical 模型：字段沿用源里的索引与源语义，
//     索引 → 稳定 ID 的映射属 B5，不在这里做。
//   · 不引入 Spine Runtime，不链接其模块，不复制其实现。
//
// 字节布局依据：docs/skeleton/field-mapping.md + 对语料的独立字节走查
// （tools/skeleton-probe/layout-explore.mjs，临时工具，核对后删除）。
// 走查结论：样本 02（425 字节）按本模型可精确到文件末尾（strict EOF）。

#ifndef LOONGBONES_IMPORTERS_SPINE32_SOURCE_MODEL_HPP
#define LOONGBONES_IMPORTERS_SPINE32_SOURCE_MODEL_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace loongbones::importers::spine32 {

/// 只接受这一个头版本（field-mapping.md §1：语料 1113/1113 全为 3.2.01）。
inline constexpr char kAcceptedVersion[] = "3.2.01";

/// 附件类型字节。**只 0（region）映射到 canonical**；1..5 的字节布局已走查确认、
/// 可安全遍历到 EOF，但不进 IR（只计数）。
///
/// 取值与官方 3.x **不同**，以本工作区已语料验证的 converter/src/read/binary32.ts
/// （ATTACHMENT_TYPE 常量）为准：
///   region=0, boundingbox=1, mesh=2, weightedmesh=3, linkedmesh=4, weightedlinkedmesh=5
/// 注意 3 是**加权网格**而不是 linked mesh —— 官方 3.5+ 的枚举里 Linkedmesh=3，
/// 按那个顺序读会在 630 个文件上错位（已证伪）。
///
/// 独立字节证据：b0001s3 的 `L_finger`（type=3, vertexCount=40）顶点区
/// 2687..3742 共 1056 字节；按「每顶点 boneCount + 4×boneCount 个 float」解析，
/// 24 个顶点 1 根骨 + 16 个顶点 2 根骨 = 24×5 + 16×9 = 264 个 float = 1056 字节，
/// 与实测长度**精确相符**；紧随其后的 0x1b 是 hull，再往后 0x02 就是下一个
/// slotIndex。
enum class AttachmentTypeByte : std::uint8_t {
  kRegion = 0,
  kBoundingBox = 1,
  kMesh = 2,
  kWeightedMesh = 3,
  kLinkedMesh = 4,
  kWeightedLinkedMesh = 5,
};

struct SourceBone {
  std::string name;
  /// 源里的父骨骼下标；-1（或第 0 根缺省）表示根。**不是**稳定 ID。
  int parentIndex = -1;
  double rotationDeg = 0.0;
  double x = 0.0;
  double y = 0.0;
  double scaleX = 1.0;
  double scaleY = 1.0;
  double shearXDeg = 0.0;
  double shearYDeg = 0.0;
  double length = 0.0;
  /// 3.2 是两个独立布尔，**不是** 3.6+ 的 transform 枚举（field-mapping §2）。
  bool inheritScale = true;
  bool inheritRotation = true;
};

struct SourceSlot {
  std::string name;
  int boneIndex = 0;
  /// 源里是 0xRRGGBBAA 的 32 位；转 0..1 浮点属 B5 映射，不在这里做。
  std::uint32_t colorRgba = 0xFFFFFFFFu;
  std::optional<std::string> attachmentName;
  /// 源枚举 0..3（normal/additive/multiply/screen）；越界由 Reader 拒绝。
  int blend = 0;
};

struct SourceRegionAttachment {
  std::string name;
  /// 3.x 允许 path 与 name 不一致；region 查找键 = `path ?? name`（field-mapping §10.1）。
  std::optional<std::string> path;
  double rotationDeg = 0.0;
  double x = 0.0;
  double y = 0.0;
  double scaleX = 1.0;
  double scaleY = 1.0;
  double width = 0.0;
  double height = 0.0;
  std::uint32_t colorRgba = 0xFFFFFFFFu;
};

/// 一个皮肤条目（(slot 下标) → 附件列表）。
struct SourceAttachment {
  /// 皮肤里的键（skinKey）。
  std::string key;
  /// 附件自身名字；为 null 时回退到 key（语料绝大多数如此）。
  std::optional<std::string> name;
  std::uint8_t typeByte = 0;
  /// 仅 typeByte == 0 时有效。
  SourceRegionAttachment region;
};

struct SourceSkinSlot {
  int slotIndex = 0;
  std::vector<SourceAttachment> attachments;
};

struct SourceSkin {
  /// default 皮肤没有名字字段；附加皮肤才有。
  std::optional<std::string> name;
  std::vector<SourceSkinSlot> slots;
};

/// 不支持但必须"计数 + 诊断"的能力（field-mapping §11.3）。
/// 不生成伪空数据；只给统计与首个源位置，供 B5 报告。
struct UnsupportedCounts {
  std::size_t mesh = 0;
  std::size_t linkedMesh = 0;
  std::size_t weightedMesh = 0;
  std::size_t weightedLinkedMesh = 0;
  std::size_t boundingBox = 0;
  std::size_t otherAttachmentType = 0;
  std::size_t ikConstraints = 0;
  std::size_t transformConstraints = 0;
  std::size_t events = 0;
  std::size_t animations = 0;
  std::size_t timelines = 0;
  std::size_t deformTimelines = 0;
};

struct SourceSkeleton {
  /// 头里的 hash 字段：源自带标识，不是我们对文件算的摘要（field-mapping §1）。
  std::optional<std::string> hash;
  std::string version;
  double width = 0.0;
  double height = 0.0;
  bool nonessential = false;
  /// 仅 nonessential 为真时出现。
  std::optional<std::string> imagesPath;

  std::vector<SourceBone> bones;
  std::vector<SourceSlot> slots;
  std::vector<SourceSkin> skins;

  UnsupportedCounts unsupported;
};

}  // namespace loongbones::importers::spine32

#endif  // LOONGBONES_IMPORTERS_SPINE32_SOURCE_MODEL_HPP
