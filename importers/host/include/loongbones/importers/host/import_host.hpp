// Task 4 B6: 静态插件宿主 —— 注册表、候选合并校验、确认后提交。
//
// 依赖方向：`hosts/ → api/`，`spine32/ → api/ + host/`（注册入口）。
// 本库**不依赖**任何具体插件实现；`registerBuiltinPlugins()` 由插件侧提供定义。
//
// 事务语义（B6 第 3/4 条）：
//   1. prepare() 只产出候选，不改目标工程、不写盘；
//   2. commitImport() 先把暂存资源 stage+publish（不覆盖既有文件），
//      再用现有原子 saveProject() 替换 project.json；
//   3. 任一步失败 → staging.rollback() 只删本事务创建的文件，旧工程保持有效；
//   4. 进程崩溃最多遗留未引用的资源文件，**不会**留下 project.json 指向缺失文件的窗口。

#ifndef LOONGBONES_IMPORTERS_HOST_IMPORT_HOST_HPP
#define LOONGBONES_IMPORTERS_HOST_IMPORT_HOST_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "loongbones/format/project_document.hpp"
#include "loongbones/importers/api/import_plugin.hpp"

namespace loongbones::importers::host {

/// 静态插件注册表。**无动态库、无 ABI**：插件在链接期进入可执行文件。
///
/// 注：`IResourceResolver` / `IResourceStaging` 的**文件系统实现**由各导入插件
/// 提供（见 `importers/spine32/…/atlas_import.hpp`），宿主只依赖接口 ——
/// 这样宿主库不必链接任何具体插件，插件的注册入口反过来依赖宿主。
class ImportRegistry {
 public:
  [[nodiscard]] static ImportRegistry& instance();

  void registerPlugin(std::unique_ptr<api::IImportPlugin> plugin);
  void clear() noexcept;

  /// 精确匹配 `formatId` 与 `formatVersion`；不匹配返回 nullptr。
  [[nodiscard]] const api::IImportPlugin* find(std::string_view formatId,
                                              std::string_view formatVersion) const noexcept;

  /// 按注册顺序返回全部 descriptor。
  [[nodiscard]] std::vector<const api::ImportDescriptor*> descriptors() const;

 private:
  ImportRegistry() = default;
  std::vector<std::unique_ptr<api::IImportPlugin>> plugins_;
};

/// 把本可执行文件内置的插件注册进 `registry`。
///
/// 定义在**插件侧**（`importers/spine32/src/builtin_registration.cpp`），以便依赖方向
/// 始终是「插件 → 宿主」，而不是反过来。之所以不用「静态初始化器自动注册」：
/// 静态库里只有初始化器的目标文件不会被链接器拉进来，注册会静默丢失。
void registerBuiltinPlugins(ImportRegistry& registry);

struct ImportCommitOptions {
  std::string projectDirectory;
  std::string appVersion;
  bool acceptWarnings = false;
};

struct ImportCommitOutcome {
  bool committed = false;
  /// draft 的诊断 + 提交期新增的诊断。
  std::vector<api::ImportDiagnostic> diagnostics;
};

/// 提交一份 prepare() 出来的候选。**这是唯一会改磁盘的入口。**
///
/// 前置条件：draft.valid 为真且无 error 级诊断；有 warning 时必须
/// `acceptWarnings` 为真。任何不满足都会原样返回且不写任何东西。
ImportCommitOutcome commitImport(const api::ImportDraft& draft, const ImportCommitOptions& options,
                                 api::IResourceStaging& staging,
                                 format::ProjectDocument& inOutDocument);

}  // namespace loongbones::importers::host

#endif  // LOONGBONES_IMPORTERS_HOST_IMPORT_HOST_HPP
