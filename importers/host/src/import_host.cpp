// Task 4 B6: 静态插件宿主的注册表与提交事务实现。
//
// 边界：宿主**只依赖 `api/` 的接口**。文件系统实现由导入插件提供，
// 因此宿主库不会反向依赖任何具体插件。

#include "loongbones/importers/host/import_host.hpp"

#include <string>
#include <utility>
#include <vector>

#include "loongbones/format/format_diagnostics.hpp"
#include "loongbones/format/project_io.hpp"

namespace loongbones::importers::host {
namespace {

void addDiag(std::vector<api::ImportDiagnostic>& out, api::ImportCode code,
             api::ImportSeverity severity, std::string message, std::string location) {
  out.push_back({code, severity, std::move(message), std::move(location)});
}

}  // namespace

ImportRegistry& ImportRegistry::instance() {
  static ImportRegistry registry;
  return registry;
}

void ImportRegistry::registerPlugin(std::unique_ptr<api::IImportPlugin> plugin) {
  if (plugin != nullptr) plugins_.push_back(std::move(plugin));
}

void ImportRegistry::clear() noexcept { plugins_.clear(); }

const api::IImportPlugin* ImportRegistry::find(std::string_view formatId,
                                              std::string_view formatVersion) const noexcept {
  for (const auto& plugin : plugins_) {
    if (plugin->canImport(formatId, formatVersion)) return plugin.get();
  }
  return nullptr;
}

std::vector<const api::ImportDescriptor*> ImportRegistry::descriptors() const {
  std::vector<const api::ImportDescriptor*> out;
  out.reserve(plugins_.size());
  for (const auto& plugin : plugins_) out.push_back(&plugin->descriptor());
  return out;
}

ImportCommitOutcome commitImport(const api::ImportDraft& draft, const ImportCommitOptions& options,
                                 api::IResourceStaging& staging,
                                 format::ProjectDocument& inOutDocument) {
  ImportCommitOutcome outcome;
  outcome.diagnostics = draft.report.diagnostics;

  // ---- 前置闸门：invalid / 有 error / warning 未确认 → 一个字节都不写 ----
  if (!draft.valid || draft.report.hasErrors()) return outcome;
  if (draft.report.hasWarnings() && !options.acceptWarnings) {
    addDiag(outcome.diagnostics, api::ImportCode::kWarningsNotAccepted,
            api::ImportSeverity::kError, "存在未确认的 warning，需调用方显式接受后方可提交导入",
            "import");
    return outcome;
  }

  // ---- 1) 暂存资源（写临时文件 + 回读比对） ----
  std::string error;
  for (const api::StagedResource& resource : draft.resources) {
    if (!staging.stage(resource.projectLocation, resource.bytes, error)) {
      staging.rollback();
      addDiag(outcome.diagnostics, api::ImportCode::kResourceStageFailed,
              api::ImportSeverity::kError, "资源暂存失败: " + error, resource.projectLocation);
      return outcome;
    }
  }

  // ---- 2) 发布资源（原子落到最终路径，拒绝覆盖） ----
  if (!staging.publish(error)) {
    staging.rollback();
    addDiag(outcome.diagnostics, api::ImportCode::kResourceStageFailed,
            api::ImportSeverity::kError, "资源暂存失败: " + error, "assets");
    return outcome;
  }

  // ---- 3) 原子替换 project.json（资源已就位，无悬挂窗口） ----
  const format::FormatDiagnostic saved =
      format::saveProject(options.projectDirectory, draft.document, options.appVersion);
  if (saved.code != format::FormatDiagnosticCode::kNone) {
    staging.rollback();
    addDiag(outcome.diagnostics, api::ImportCode::kProjectSaveFailed,
            api::ImportSeverity::kError, "工程保存失败: " + saved.message,
            format::kProjectFileName);
    return outcome;
  }

  staging.complete();
  inOutDocument = draft.document;
  outcome.committed = true;
  return outcome;
}

}  // namespace loongbones::importers::host
