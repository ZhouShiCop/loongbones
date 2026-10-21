// Task 4 B1: minimal Atlas + PNG import vertical slice.
// Spine source data is read-only; this API emits only Loongbones domain data.
#ifndef LOONGBONES_IMPORTERS_SPINE32_ATLAS_IMPORT_HPP
#define LOONGBONES_IMPORTERS_SPINE32_ATLAS_IMPORT_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "loongbones/domain/ids.hpp"
#include "loongbones/format/project_document.hpp"
#include "loongbones/importers/api/import_plugin.hpp"

namespace loongbones::importers::spine32 {

enum class AtlasImportSeverity { kWarning, kError };
enum class AtlasImportCode {
  kNone,
  kAtlasSyntax,
  kAtlasValueInvalid,
  kAtlasFeatureUnsupported,
  kSourceResourceMissing,
  kSourcePathRejected,
  kPngInvalid,
  kDomainRejected,
  kWarningsNotAccepted,
  kResourceStageFailed,
  kProjectSaveFailed,
};

struct AtlasImportDiagnostic {
  AtlasImportCode code = AtlasImportCode::kNone;
  AtlasImportSeverity severity = AtlasImportSeverity::kError;
  std::string message;
  std::string sourceLocation;
};

struct PngInfo {
  std::int32_t width = 0;
  std::int32_t height = 0;
};

struct StagedTexture {
  std::string projectLocation;
  std::vector<std::uint8_t> bytes;
};

struct AtlasImportDraft {
  format::ProjectDocument document;
  std::vector<StagedTexture> textures;
  std::vector<AtlasImportDiagnostic> diagnostics;
  std::size_t pageCount = 0;
  std::size_t regionCount = 0;
  bool valid = false;
};

// B6 起，导入契约（插件接口 / 宿主服务接口 / 报告 / 取消 / 进度）的权威定义在
// `importers/api/import_plugin.hpp`。这里保留 B1 时期的原名别名，
// 使 B1 的既有代码与测试**无需改动**——它们仍按 `spine32::IResourceResolver`
// 这样的名字引用，解析到的是同一套类型。
using IResourceResolver = api::IResourceResolver;
using IIdFactory = api::IIdFactory;
using RandomIdFactory = api::RandomIdFactory;

using IResourceStaging = api::IResourceStaging;
using StagedResource = api::StagedResource;

/// Read the PNG signature and IHDR (including its CRC); does not decode pixels.
bool inspectPng(const std::vector<std::uint8_t>& bytes, PngInfo& info,
                std::string& error);

/// Parse atlas text, resolve each page image through the read-only resolver, validate a
/// complete candidate ProjectData, and return a draft. No target project or disk is changed.
AtlasImportDraft prepareAtlasImport(const format::ProjectDocument& current,
                                   std::string_view atlasText,
                                   std::string_view atlasLogicalPath,
                                   const IResourceResolver& resolver,
                                   IIdFactory& ids);

/// Commit a prepared candidate. Warnings require explicit acceptance. New texture files are
/// published first; saveProject() atomically replaces project.json; failures roll back assets.
std::vector<AtlasImportDiagnostic> commitAtlasImport(
    const AtlasImportDraft& draft, const std::string& projectDirectory,
    const std::string& appVersion, bool acceptWarnings, IResourceStaging& staging,
    format::ProjectDocument& inOutDocument);

/// Host-side directory resolver. `authorizedRoot` is the only readable tree; the importer
/// supplies normalized root-relative paths. Canonical paths and symlinks are checked.
class FileSystemResourceResolver final : public IResourceResolver {
 public:
  explicit FileSystemResourceResolver(std::string authorizedRoot);
  bool read(std::string_view sourceRelativePath, std::vector<std::uint8_t>& bytes,
            std::string& error) const override;
  [[nodiscard]] bool valid() const noexcept { return valid_; }

 private:
  std::string root_;
  bool valid_ = false;
};

/// Host-side resource transaction. Stage writes temporary files; publish atomically installs
/// them without overwriting; rollback removes only files created by this transaction.
class DirectoryResourceStaging final : public IResourceStaging {
 public:
  explicit DirectoryResourceStaging(std::string projectRoot);
  ~DirectoryResourceStaging() override;
  bool stage(std::string_view projectRelativePath,
             const std::vector<std::uint8_t>& bytes, std::string& error) override;
  bool publish(std::string& error) override;
  void rollback() noexcept override;
  void complete() noexcept override;
  [[nodiscard]] bool valid() const noexcept { return valid_; }

 private:
  struct Entry {
    std::string temporary;
    std::string target;
    bool published = false;
  };
  std::string root_;
  std::vector<Entry> entries_;
  bool valid_ = false;
  bool completed_ = false;
};

const char* toString(AtlasImportCode code) noexcept;

}  // namespace loongbones::importers::spine32

#endif  // LOONGBONES_IMPORTERS_SPINE32_ATLAS_IMPORT_HPP
