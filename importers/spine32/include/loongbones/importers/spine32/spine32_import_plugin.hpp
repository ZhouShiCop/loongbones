// Task 4 B6: Spine 3.2.01 二进制导入插件（`spine.binary@3.2.01`）。
//
// 一次导入 = 一个「单位」= `.skel.bytes` + 同名 `.atlas.txt` + 其页引用的 PNG。
// 插件只读源、只产出候选；**不写盘、不改目标工程**（B6 第 2 条）。
//
// 边界（承接 B4/B5 的裁定；Task 5 B3/B4 更新）：
//   · 进入 canonical 的有：头、骨骼、插槽、皮肤骨架、**region / mesh / 加权 mesh /
//     linked mesh 附件**、**IK / transform 约束**、图集/页/区域、纹理资产
//     （PNG 只取 IHDR 像素尺寸，不解码像素）；
//   · boundingbox（语料 0 例，D32 只留类型占位）/ 动画时间线 / deform / 事件
//     **只计数 + warning**，不建任何 Domain 对象。计数随报告带出，
//     用户能数出丢了什么、丢了多少。

#ifndef LOONGBONES_IMPORTERS_SPINE32_SPINE32_IMPORT_PLUGIN_HPP
#define LOONGBONES_IMPORTERS_SPINE32_SPINE32_IMPORT_PLUGIN_HPP

#include <cstdint>

#include "loongbones/domain/diagnostics.hpp"
#include "loongbones/importers/api/import_plugin.hpp"

namespace loongbones::importers::spine32 {

constexpr const char* kSpine32PluginId = "loongbones.spine32";
constexpr const char* kSpine32FormatId = "spine.binary";
constexpr const char* kSpine32FormatVersion = "3.2.01";
/// 插件自身的版本号（写进 `SourceRef.importedByPluginVersion`，供溯源）。
constexpr std::int64_t kSpine32PluginVersion = 1;

/// Domain 诊断码 → 导入诊断码（Task 5 B4）。
///
/// 第二批（mesh / 蒙皮 / 约束）的 9 个域码必须能被宿主按**稳定码**判断，而不是按文案：
///   · **引用类**（绑定骨 / linked 父附件 / 约束目标骨未解析）→ `kUnresolvedReference`
///     （与「附件 → region 未命中」同类：源里的引用在目标工程里落不了地）；
///   · **取值类**（三角索引越界 / UV 越界 / 权重和非法 / 绑定为空 / IK 链为空）
///     → `kParseError`（源数据本身非法，不是宿主环境问题）。
/// 其余域码 → `kDomainRejected`。
api::ImportCode domainDiagnosticToImportCode(DiagnosticCode code) noexcept;

class Spine32ImportPlugin final : public api::IImportPlugin {
 public:
  [[nodiscard]] const api::ImportDescriptor& descriptor() const noexcept override;
  [[nodiscard]] api::ImportDraft prepare(const api::ImportRequest& request) const override;
};

}  // namespace loongbones::importers::spine32

#endif  // LOONGBONES_IMPORTERS_SPINE32_SPINE32_IMPORT_PLUGIN_HPP
