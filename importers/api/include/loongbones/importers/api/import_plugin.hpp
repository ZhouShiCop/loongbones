// Task 4 B6: 导入插件 API 与静态宿主契约。
//
// 纪律（Task4_Implementation_Plan.md B6）：
//   1. `IImportPlugin` 只使用**值语义** canonical 类型与宿主抽象；**不创建运行时
//      动态库 / ABI**（静态链接、静态注册）。descriptor 匹配 `spine.binary@3.2.01`。
//   2. `prepare()` 先产出 `ImportDraft` 与报告，**完全不改**目标工程；
//      error 直接拒绝；warning 需调用方显式接受后才可继续。
//   3. 提交走**完整候选副本**：复制现有 ProjectData，追加对象，用临时
//      `Project::adoptData()` 校验全工程；不在解析过程中增量调用 Project 写 API。
//   4. PNG 由宿主暂存服务写到工程内相对路径。先写临时资源并回读校验，
//      候选通过后再落资源文件，最后用现有原子 `saveProject()` 替换 project.json；
//      失败回滚新增文件，旧工程保持有效。
//   5. 无 GUI 期间 `tools/skeleton-import-smoke` 作为最小宿主；不复制或提交真实游戏输入。
//
// 边界提醒（承接 B4/B5）：本层只提交**已映射**的 canonical 数据。mesh 族、
// 动画时间线、IK/transform 参数仍不在 canonical 范围内，它们以「能力计数 + warning」
// 的形式出现在 `ImportReport::unsupported` 里 —— 静默丢弃等于运行时缺件。

#ifndef LOONGBONES_IMPORTERS_API_IMPORT_PLUGIN_HPP
#define LOONGBONES_IMPORTERS_API_IMPORT_PLUGIN_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "loongbones/domain/ids.hpp"
#include "loongbones/format/project_document.hpp"

namespace loongbones::importers::api {

enum class ImportSeverity { kWarning, kError };

/// 导入诊断码。**码名稳定**，程序逻辑按枚举分支，不按文案。
enum class ImportCode {
  kNone = 0,
  kSourceMissing,          ///< 主源（`.skel.bytes`）读不到
  kSourceRejected,         ///< 源路径未通过授权检查
  kParseError,             ///< Reader 结构性失败
  kResourceMissing,        ///< 伴随资源（`.atlas.txt` / PNG）不存在
  kResourceInvalid,        ///< PNG 头 / IHDR / CRC / 尺寸非法
  kUnresolvedReference,    ///< 附件 → region 未命中
  kAttachmentRegionDegraded,  ///< 精确未命中、按 `trim()` 降级命中（warning，T-B3）
  kUnsupportedCapability,  ///< 源里有、本阶段不导入
  kDomainRejected,         ///< 候选工程过不了 Domain 全量校验
  kResourceStageFailed,    ///< 资源暂存 / 发布失败
  kProjectSaveFailed,      ///< project.json 保存失败
  kWarningsNotAccepted,    ///< 存在 warning 但调用方未显式接受
  kCancelled,              ///< 调用方取消
  /// Task 6：目录内存在**多个**候选伴随资源（如多个 `.atlas.txt`）。
  /// 必须**显式拒绝 + 可定位诊断**，不得猜测、不得选第一个。
  kResourceAmbiguous,
};

struct ImportDiagnostic {
  ImportCode code = ImportCode::kNone;
  ImportSeverity severity = ImportSeverity::kError;
  std::string message;
  /// 位置：源内段名 / 相对路径 / `project.json` 等。
  std::string location;
};

struct ImportReport {
  std::vector<ImportDiagnostic> diagnostics;

  /// 源里存在、本阶段**不导入**的能力 → 计数。「不得静默丢失」的账本：
  /// 丢了多少必须能数出来，而不是无声消失。
  std::vector<std::pair<std::string, std::size_t>> unsupported;

  /// 将落地的 canonical 对象 → 计数。保存→重开后必须与之一致。
  std::vector<std::pair<std::string, std::size_t>> counts;

  [[nodiscard]] std::size_t errorCount() const noexcept;
  [[nodiscard]] std::size_t warningCount() const noexcept;
  [[nodiscard]] bool hasErrors() const noexcept { return errorCount() != 0; }
  [[nodiscard]] bool hasWarnings() const noexcept { return warningCount() != 0; }
};

/// 进度回调。可为 null（静默）。
class IProgressSink {
 public:
  virtual ~IProgressSink() = default;
  virtual void report(std::string_view stage, std::size_t done, std::size_t total) = 0;
};

/// 取消令牌。可为 null（不可取消）。
class ICancelToken {
 public:
  virtual ~ICancelToken() = default;
  [[nodiscard]] virtual bool isCancelled() const noexcept = 0;
};

// ---- 宿主服务抽象（由宿主实现，插件只依赖接口） ----

/// 只读资源解析：给出**授权根相对**路径，返回字节。
class IResourceResolver {
 public:
  virtual ~IResourceResolver() = default;
  [[nodiscard]] virtual bool read(std::string_view sourceRelativePath,
                                  std::vector<std::uint8_t>& bytes, std::string& error) const = 0;

  /// 列举**源所在目录**下的文件名：只一层、**不递归**、只返回文件名（不含路径）、
  /// 按名称**确定性排序**。插件**不得**据此推断真实文件系统路径。
  ///
  /// Task 6（共享图集布局）新增的**可选**能力：默认实现返回 **false**（"不支持列举"），
  /// 宿主可覆盖。插件在列举不可用时**退化为**原「骨架同名」解析，
  /// **不得**因此放宽授权根校验 —— 这是本接口的硬边界。
  ///
  /// 失败分类沿用 B6 三种：目录不存在/不可读 → 资源无法解析；
  /// 越出授权根 / 含 `..` / 绝对路径 / 符号链接逃逸 → 越出授权根。
  [[nodiscard]] virtual bool listDirectory(std::string_view sourceDirectory,
                                           std::vector<std::string>& names,
                                           std::string& error) const {
    (void)sourceDirectory;
    (void)names;
    error = "资源解析不支持目录列举";
    return false;
  }
};

/// 工程内资源写入事务：stage → publish → complete / rollback。
class IResourceStaging {
 public:
  virtual ~IResourceStaging() = default;
  virtual bool stage(std::string_view projectRelativePath, const std::vector<std::uint8_t>& bytes,
                     std::string& error) = 0;
  /// 暂存文件原子落到最终路径；**不得覆盖**已存在文件。
  virtual bool publish(std::string& error) = 0;
  /// 删除本事务创建的临时与目标文件；**不得删除任何既有文件**。
  virtual void rollback() noexcept = 0;
  /// project.json 已提交成功后调用，确认保留已发布文件。
  virtual void complete() noexcept = 0;
};

/// 所有持久化 ID 的**唯一**来源。
class IIdFactory {
 public:
  virtual ~IIdFactory() = default;
  virtual Id create(IdKind kind) = 0;
};

class RandomIdFactory final : public IIdFactory {
 public:
  Id create(IdKind kind) override { return Id::generate(kind); }
};

// ---- 请求 / 草案 ----

struct ImportDescriptor {
  std::string pluginId;
  std::string formatId;
  std::string formatVersion;
};

struct StagedResource {
  /// 工程内相对路径，例如 `assets/textures/<id>.png`。
  std::string projectLocation;
  std::vector<std::uint8_t> bytes;
};

/// 一次导入的**候选**结果。尚未写入任何东西。
struct ImportDraft {
  ImportDescriptor descriptor;
  format::ProjectDocument document;
  std::vector<StagedResource> resources;
  ImportReport report;
  bool valid = false;
};

struct ImportRequest {
  format::ProjectDocument current;
  /// 归一化后的**授权根相对**路径，例如 `h3181c2/h3181c2.skel.bytes`。
  std::string sourceLogicalPath;
  IResourceResolver& resolver;
  IIdFactory& ids;
  IProgressSink* progressSink = nullptr;
  const ICancelToken* cancel = nullptr;

  [[nodiscard]] bool cancelled() const noexcept {
    return cancel != nullptr && cancel->isCancelled();
  }
  void progress(std::string_view stage, std::size_t done, std::size_t total) const {
    if (progressSink != nullptr) progressSink->report(stage, done, total);
  }
};

class IImportPlugin {
 public:
  virtual ~IImportPlugin() = default;
  [[nodiscard]] virtual const ImportDescriptor& descriptor() const noexcept = 0;

  [[nodiscard]] bool canImport(std::string_view formatId,
                               std::string_view formatVersion) const noexcept {
    const ImportDescriptor& d = descriptor();
    return d.formatId == formatId && d.formatVersion == formatVersion;
  }

  /// 解析源并产出候选。**不得**修改 `request.current`，**不得**写盘。
  [[nodiscard]] virtual ImportDraft prepare(const ImportRequest& request) const = 0;
};

const char* toString(ImportCode code) noexcept;

}  // namespace loongbones::importers::api

#endif  // LOONGBONES_IMPORTERS_API_IMPORT_PLUGIN_HPP
