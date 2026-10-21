// 原生项目格式 v1 的解码 / 编码（Task 3 阶段 B）
//
// 依赖方向：format -> domain。本模块**不向 Domain 泄漏任何 JSON 概念**。

#ifndef LOONGBONES_FORMAT_CODEC_HPP
#define LOONGBONES_FORMAT_CODEC_HPP

#include <string>
#include <vector>

#include "loongbones/domain/diagnostics.hpp"
#include "loongbones/domain/project.hpp"
#include "loongbones/format/format_diagnostics.hpp"
#include "loongbones/format/json_reader.hpp"
#include "loongbones/format/project_document.hpp"

namespace loongbones {
namespace format {

/// 写出格式的版本（RFC §5.1；Task 4 B3 起统一为 v2）
constexpr int kFormatVersion = 2;
/// 本 Reader 能读取的最高格式版本（RFC §1.1）：同时接受 v1 与 v2
constexpr int kReaderMaxVersion = 2;
/// 本 Reader 能读取的最低格式版本：v1 仍可读入后迁移
constexpr int kReaderMinVersion = 1;
constexpr int kLegacyV1Version = 1;
constexpr const char* kFormatId = "loongbones.native-project";

/// v1 基线自带的能力，无需列入 capabilities.required（RFC §1.2）
constexpr const char* kBaselineCapability = "loongbones.atlas.v1";
/// 骨架能力：v2 文档**必须**把本项列入 capabilities.required
/// （负责人 2026-10-20 裁定：project.skeletons 是 v2 必需键，即使为空数组，
///  因此"有 skeletons[] 的 v2 文档"= 全部 v2 文档）。
///
/// 处理顺序保证：能力检查先于任何骨架数据解码，故不支持本能力的旧 Reader
/// 报 kCapabilityUnsupported，而不是骨架字段的类型/结构错误。
constexpr const char* kSkeletonCapability = "loongbones.skeleton.v1";

/// 加载失败的来源（RFC §9.2：Format 任一环节失败即中止，不进入 Domain）
enum class DiagSource { kNone, kFormat, kDomain };

struct LoadResult {
  bool ok = false;
  ProjectDocument document;
  DiagSource source = DiagSource::kNone;
  /// source == kFormat 时有效
  FormatDiagnostic formatDiag;
  /// source == kDomain 时有效。
  ///
  /// 已知限制：Project::adoptData() 内部会对候选执行 validate()，但只把**第一条**
  /// 违规放进 Status 返回（ProjectData 成员私有，没有公开途径对候选再调 validate()）。
  /// 故这里最多一条，且携带 Domain 原有的 location。
  std::vector<Diagnostic> domainDiags;
};

/// 从文档字节解码为 ProjectDocument。
///
/// 流程（RFC §6.5.2）：解析 -> 结构/版本/能力 -> 构造候选 -> Project::adoptData() 校验。
/// 任一步失败立即返回，**不产生半保留的文档对象**。
///
/// B3 起同时接受 v1 与 v2（Task4_Implementation_Plan.md B3 第 2 条）：
/// 先判 formatVersion，再判 minimumReaderVersion，再查 required capabilities。
/// v1 文档在完整解码/校验后被**迁移**为内存中的 v2 结构（见 migrateV1ToV2），
/// 磁盘文件本身不被触碰 —— 只有用户显式保存时才写 v2。
LoadResult decodeDocument(const std::string& text, const ReaderLimits& limits);

/// 把 v1 文档**纯函数地**迁移为内存中的 v2 文档（B3 第 3 条）。
///
/// 只做三件事：
///   1. 确保 `data.skeletons` 存在（v1 没有该集合，故为空数组）；
///   2. 设置 v2 的能力字段（`capabilities.required` 含 kSkeletonCapability）；
///   3. 标记 `sourceVersion = kV1`。
///
/// 不生成任何 ID、不改写任何既有字段（含未知字段与 optional 能力）、不碰磁盘。
/// 幂等：对已是 v2 的文档调用不产生任何变化。
void migrateV1ToV2(ProjectDocument& document);

/// 把 ProjectDocument 编码为确定性 JSON 文本（RFC §4.5）。
/// 已知数值按类型化值重新序列化；未知数值**逐字节回写原始字面量**（RFC §6.6）。
/// appVersion 写出时提供（U4）；传空串表示省略。
std::string encodeDocument(const ProjectDocument& doc, const std::string& appVersion);

}  // namespace format
}  // namespace loongbones

#endif  // LOONGBONES_FORMAT_CODEC_HPP
