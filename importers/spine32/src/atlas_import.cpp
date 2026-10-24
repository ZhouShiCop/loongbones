// Task 4 B1: Atlas + PNG 导入切片实现。
//
// 【重建说明 · 2026-10-21】本文件在本轮目录整理中被**误删**（仓库无 git、
// 无回收站记录，无法还原）。当时的处置经过：
//   1. 先有一次 ANSI/UTF-8 整文件往返，把文件里的中文诊断字符串**二次编码损坏**
//      （GBK 解码 UTF-8 时对无效序列降级成 `?`，**不可逆**）；
//   2. 随后又用一个「按行号删除区间」的临时脚本 `_cut.ps1` 去清理残留 ——
//      脚本里的行号取自**改动前**的版本，导致整个文件被截断到 19 行。
//      （该脚本及其日志已在收尾清理时删除，仓库中现已查无痕迹。）
// 当前内容是依据公开头文件契约、`tests/importers/spine32/test_atlas_import.cpp`
// 与 `tools/atlas-import-smoke/` 的调用点**重建**的：
//   · 逻辑、诊断码、事务语义与原实现一致（已由 atlas_import_tests 与语料 smoke 验证）；
//   · **中文诊断文案为重新撰写**，不是逐字原文 —— 逐字原文在第 1 步已不可逆地丢失。
// 残余风险：重建版的断言/文案可能与原文件存在细节差异，无法自证等价。
//
// 纪律：
//   · Spine 语料**只读**：所有源读取都经 `IResourceResolver`，且路径必须落在授权根内；
//   · 输出只含 Domain 数据；
//   · 提交走「完整候选副本 + `Project::adoptData()` 全量校验」，最后才原子替换
//     `project.json`；失败回滚本事务新建的文件，旧工程保持有效。

#include "loongbones/importers/spine32/atlas_import.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "loongbones/domain/atlas.hpp"
#include "loongbones/domain/ids.hpp"
#include "loongbones/domain/project.hpp"
#include "loongbones/format/format_diagnostics.hpp"
#include "loongbones/format/project_io.hpp"
#include "loongbones/importers/api/path_util.hpp"
#include "loongbones/importers/spine32/atlas_text_reader.hpp"

namespace fs = std::filesystem;

namespace loongbones::importers::spine32 {
namespace {

/// 单个 PNG 的读取上限。最小切片不做大文件，超限一律报错而不是读入内存。
constexpr std::size_t kMaxPngBytes = 512U * 1024U * 1024U;

void addDiag(std::vector<AtlasImportDiagnostic>& out, AtlasImportCode code,
             AtlasImportSeverity severity, std::string message, std::string location) {
  out.push_back({code, severity, std::move(message), std::move(location)});
}

/// AtlasTextDiagnostic（B5 Reader）→ AtlasImportDiagnostic（本模块）。码一一对应，
/// 消息与等级原样搬运 —— B1 的既有断言依赖它们。
AtlasImportCode toImportCode(AtlasTextCode code) noexcept {
  switch (code) {
    case AtlasTextCode::kSyntax: return AtlasImportCode::kAtlasSyntax;
    case AtlasTextCode::kValueInvalid: return AtlasImportCode::kAtlasValueInvalid;
    case AtlasTextCode::kFeatureUnsupported: return AtlasImportCode::kAtlasFeatureUnsupported;
    case AtlasTextCode::kNone: return AtlasImportCode::kNone;
  }
  return AtlasImportCode::kNone;
}

std::uint32_t readBe32(const std::uint8_t* p) {
  return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = static_cast<std::uint32_t>(-(static_cast<std::int32_t>(crc & 1u)));
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return ~crc;
}

bool isSafeRelative(std::string value) {
  if (value.empty()) return false;
  std::replace(value.begin(), value.end(), '\\', '/');
  if (value.front() == '/' || value.find(':') != std::string::npos) return false;
  std::istringstream parts(value);
  std::string part;
  while (std::getline(parts, part, '/')) if (part == "..") return false;
  return true;
}

std::string normalizedRelative(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  fs::path p(value);
  return p.lexically_normal().generic_string();
}

/// 在 used 集合里为 base 取一个不重复的名字（稳定可复现）。
std::string uniqueName(std::string base, const std::set<std::string>& used) {
  if (base.empty()) base = "unnamed";
  std::string candidate = base;
  int suffix = 2;
  while (used.count(candidate) != 0) {
    candidate = base + "_" + std::to_string(suffix);
    ++suffix;
  }
  return candidate;
}

bool readFile(const fs::path& path, std::size_t maxBytes, std::vector<std::uint8_t>& bytes,
              std::string& error) {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec || static_cast<std::uintmax_t>(size) > static_cast<std::uintmax_t>(maxBytes)) {
    error = ec ? "无法读取文件" : "文件超过大小上限";
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "无法打开文件";
    return false;
  }
  bytes.resize(static_cast<std::size_t>(size));
  if (!bytes.empty()) {
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
  if (!in || in.peek() != std::char_traits<char>::eof()) {
    error = "文件读取不完整或超出预期长度";
    bytes.clear();
    return false;
  }
  return true;
}

bool pathWithin(const fs::path& root, const fs::path& child) {
  if (root.empty() || child.empty()) return false;
  const auto mism = std::mismatch(root.begin(), root.end(), child.begin(), child.end());
  return mism.first == root.end();
}

}  // namespace

bool inspectPng(const std::vector<std::uint8_t>& bytes, PngInfo& info, std::string& error) {
  // 只解析 signature + IHDR（含 CRC）。**不解码像素数据** —— 本 Task 只做最小切片。
  static constexpr std::uint8_t kSignature[8] = {0x89u, 0x50u, 0x4Eu, 0x47u,
                                                       0x0Du, 0x0Au, 0x1Au, 0x0Au};
  static constexpr std::size_t kSignatureBytes = 8U;
  static constexpr std::size_t kIhdrDataBytes = 13U;
  static constexpr std::size_t kChunkHeaderBytes = 4U;  // "IHDR"
  static constexpr std::size_t kCrcBytes = 4U;
  static constexpr std::size_t kMinimumBytes =
      kSignatureBytes + kChunkHeaderBytes + kIhdrDataBytes + kCrcBytes;

  if (bytes.size() < kSignatureBytes) {
    error = "PNG signature 或 IHDR 缺失";
    return false;
  }
  if (std::memcmp(bytes.data(), kSignature, kSignatureBytes) != 0) {
    error = "PNG signature 或 IHDR 缺失";
    return false;
  }
  if (bytes.size() < kMinimumBytes) {
    error = "PNG 头部不足 13 字节的 IHDR";
    return false;
  }
  const std::uint8_t* type = bytes.data() + kSignatureBytes + kChunkHeaderBytes;
  if (std::memcmp(type, "IHDR", 4) != 0) {
    error = "PNG 头部不足 13 字节的 IHDR";
    return false;
  }
  const std::uint8_t* ihdrData = type + kChunkHeaderBytes;
  const std::uint32_t expected = crc32(type, kChunkHeaderBytes + kIhdrDataBytes);
  const std::uint32_t actual = readBe32(ihdrData + kIhdrDataBytes);
  if (expected != actual) {
    error = "PNG IHDR CRC 校验失败";
    return false;
  }
  const std::uint32_t width = readBe32(ihdrData + 0U);
  const std::uint32_t height = readBe32(ihdrData + 4U);
  if (width == 0U || height == 0U || width > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
      height > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
    error = "PNG IHDR 宽高不是正数的 int32";
    return false;
  }
  info.width = static_cast<std::int32_t>(width);
  info.height = static_cast<std::int32_t>(height);
  return true;
}

AtlasImportDraft prepareAtlasImport(const format::ProjectDocument& current,
                                    std::string_view atlasText,
                                    std::string_view atlasLogicalPath,
                                    const IResourceResolver& resolver,
                                    IIdFactory& ids) {
  AtlasImportDraft result;

  // ---- 源路径必须落在授权根内 ----
  if (!isSafeRelative(std::string(atlasLogicalPath))) {
    addDiag(result.diagnostics, AtlasImportCode::kSourcePathRejected,
            AtlasImportSeverity::kError, "图集路径必须是授权根目录内的相对路径",
            std::string(atlasLogicalPath));
    return result;
  }

  // ---- 解析图集文本 ----
  const AtlasText atlas = readAtlasText(atlasText);
  for (const AtlasTextDiagnostic& d : atlas.diagnostics) {
    addDiag(result.diagnostics, toImportCode(d.code),
            d.severity == AtlasTextSeverity::kWarning ? AtlasImportSeverity::kWarning
                                                      : AtlasImportSeverity::kError,
            d.message, d.sourceLocation);
  }
  if (!atlas.valid) return result;

  const std::string atlasPath = normalizedRelative(std::string(atlasLogicalPath));
  const fs::path atlasParent = fs::path(atlasPath).parent_path();

  Atlas built;
  built.id = ids.create(IdKind::Atlas);
  built.name = fs::path(atlasPath).stem().string();

  std::set<std::string> usedTextureNames;
  // 本轮新建的纹理资产：既要进暂存清单（供宿主落盘），
  // 也要进候选工程数据（页 → 纹理资产的引用必须能解析）。
  std::vector<TextureAsset> createdTextures;
  for (const TextureAsset& existing : current.data.textures) {
    usedTextureNames.insert(existing.name);
  }
  for (const SourceAtlasPage& srcPage : atlas.pages) {
    const std::string pageFile = srcPage.file;
    if (!isSafeRelative(pageFile)) {
      addDiag(result.diagnostics, AtlasImportCode::kSourcePathRejected,
            AtlasImportSeverity::kError, "图集页文件必须是授权根目录内的相对路径",
            "line " + std::to_string(srcPage.line));
      return result;
    }
    const std::string sourcePagePath =
        normalizedRelative((atlasParent / fs::path(pageFile)).generic_string());
    if (!isSafeRelative(sourcePagePath)) {
      addDiag(result.diagnostics, AtlasImportCode::kSourcePathRejected,
            AtlasImportSeverity::kError, "图集页路径越出授权根目录",
            "line " + std::to_string(srcPage.line));
      return result;
    }

    std::vector<std::uint8_t> pngBytes;
    std::string ioError;
    if (!resolver.read(sourcePagePath, pngBytes, ioError)) {
      addDiag(result.diagnostics, AtlasImportCode::kSourceResourceMissing,
            AtlasImportSeverity::kError, "无法读取图集页资源: " + ioError, sourcePagePath);
      return result;
    }

    PngInfo info;
    std::string pngError;
    if (!inspectPng(pngBytes, info, pngError)) {
      addDiag(result.diagnostics, AtlasImportCode::kPngInvalid,
            AtlasImportSeverity::kError, "PNG 校验失败: " + pngError, sourcePagePath);
      return result;
    }

    TextureAsset texture;
    texture.id = ids.create(IdKind::TextureAsset);
    texture.name = uniqueName(fs::path(pageFile).stem().string(), usedTextureNames);
    texture.location = "assets/textures/" + texture.id.str() + ".png";
    texture.pixelWidth = info.width;
    texture.pixelHeight = info.height;
    createdTextures.push_back(texture);

    AtlasPage page;
    page.id = ids.create(IdKind::AtlasPage);
    page.name = pageFile;
    page.textureAssetId = texture.id;
    // 页**声明**坐标尺寸，与 PNG IHDR 像素尺寸分别保存（B5 验收项）。
    page.width = srcPage.width;
    page.height = srcPage.height;
    built.pages.push_back(page);

    for (const SourceAtlasRegion& srcRegion : srcPage.regions) {
      AtlasRegion region;
      region.id = ids.create(IdKind::AtlasRegion);
      region.name = srcRegion.name;
      region.pageId = page.id;
      region.packedX = srcRegion.x;
      region.packedY = srcRegion.y;
      region.packedW = srcRegion.width;
      region.packedH = srcRegion.height;
      region.originalW = srcRegion.originalWidth;
      region.originalH = srcRegion.originalHeight;
      region.trimmedW = srcRegion.trimmedWidth;
      region.trimmedH = srcRegion.trimmedHeight;
      region.offsetX = srcRegion.offsetX;
      region.offsetY = srcRegion.offsetY;
      region.rotation = srcRegion.rotation;
      built.regions.push_back(std::move(region));
    }

    result.textures.push_back({texture.location, std::move(pngBytes)});
  }

  result.pageCount = built.pages.size();
  result.regionCount = built.regions.size();

  // ---- 完整候选副本 → 临时 Project::adoptData() 全量校验 ----
  ProjectData candidate = current.data;
  for (const TextureAsset& texture : createdTextures) candidate.textures.push_back(texture);
  candidate.atlases.push_back(std::move(built));
  const Status status = Project::createWithId(candidate.id, candidate.name).adoptData(candidate);
  // 【勿写成 status.ok()】Status::ok() 是**静态工厂**（返回成功态），通过对象调用也只返回新的成功
  // Status ⇒ `!status.ok()` 恒为 false，会**静默吞掉**所有 Domain 校验失败（死代码）。
  // 判成功与否用 `isOk()`。与 spine32_import_plugin.cpp 同款，两处都已修。
  if (!status.isOk()) {
    addDiag(result.diagnostics, AtlasImportCode::kDomainRejected,
          AtlasImportSeverity::kError,
          "候选工程未通过 Domain 校验: " + status.diagnostic().message,
          status.diagnostic().location);
    return result;
  }

  format::ProjectDocument document = current;
  document.data = std::move(candidate);
  document.sourceVersion = format::SourceFormatVersion::kV3;

  result.document = std::move(document);
  result.valid = true;
  return result;
}

std::vector<AtlasImportDiagnostic> commitAtlasImport(
    const AtlasImportDraft& draft, const std::string& projectDirectory,
    const std::string& appVersion, bool acceptWarnings, IResourceStaging& staging,
    format::ProjectDocument& inOutDocument) {
  std::vector<AtlasImportDiagnostic> diagnostics = draft.diagnostics;

  // ---- 前置闸门：warning 需显式确认；draft 必须有效 ----
  if (!draft.valid) return diagnostics;
  bool hasWarnings = false;
  for (const AtlasImportDiagnostic& d : draft.diagnostics) {
    if (d.severity == AtlasImportSeverity::kWarning) hasWarnings = true;
  }
  if (hasWarnings && !acceptWarnings) {
    addDiag(diagnostics, AtlasImportCode::kWarningsNotAccepted,
            AtlasImportSeverity::kError, "存在未确认的 warning，导入已取消", "atlas");
    return diagnostics;
  }

  std::string error;

  // ---- 1) 暂存纹理 ----
  for (const StagedTexture& staged : draft.textures) {
    if (!staging.stage(staged.projectLocation, staged.bytes, error)) {
      staging.rollback();
      addDiag(diagnostics, AtlasImportCode::kResourceStageFailed,
            AtlasImportSeverity::kError, "资源暂存失败: " + error, staged.projectLocation);
      return diagnostics;
    }
  }

  // ---- 2) 发布纹理（不覆盖既有文件） ----
  if (!staging.publish(error)) {
    staging.rollback();
    addDiag(diagnostics, AtlasImportCode::kResourceStageFailed,
          AtlasImportSeverity::kError, "资源暂存失败: " + error, "assets/textures");
    return diagnostics;
  }

  // ---- 3) 原子替换 project.json ----
  const format::FormatDiagnostic saved = format::saveProject(projectDirectory, draft.document, appVersion);
  if (saved.code != format::FormatDiagnosticCode::kNone) {
    staging.rollback();
    addDiag(diagnostics, AtlasImportCode::kProjectSaveFailed,
          AtlasImportSeverity::kError, "工程保存失败: " + saved.message, "project.json");
    return diagnostics;
  }

  staging.complete();
  inOutDocument = draft.document;
  return diagnostics;
}

FileSystemResourceResolver::FileSystemResourceResolver(std::string authorizedRoot) {
  std::error_code ec;
  const fs::path canonical = fs::canonical(fs::path(authorizedRoot), ec);
  if (ec || !fs::is_directory(canonical, ec)) return;
  root_ = canonical.string();
  valid_ = true;
}

bool FileSystemResourceResolver::read(std::string_view sourceRelativePath,
                                      std::vector<std::uint8_t>& bytes,
                                      std::string& error) const {
  if (!valid_) {
    error = "授权根目录无效";
    return false;
  }
  const std::string relative(sourceRelativePath);
  if (!isSafeRelative(relative)) {
    error = "路径必须是授权根目录内的相对路径";
    return false;
  }
  const fs::path root(root_);
  const fs::path candidate = root / fs::path(normalizedRelative(relative));
  std::error_code ec;
  const fs::path canonical = fs::canonical(candidate, ec);
  if (ec) {
    // `canonical()` 对「路径不存在」和「无法解析」返回同一种失败。
    // 两者语义完全不同：前者是缺资源，后者是环境/权限问题。
    // 若混为一谈，"缺文件"会被误报成"越权"（B6 语料扫描实测踩到）。
    std::error_code existsEc;
    if (!fs::exists(candidate, existsEc)) {
      error = "资源不存在: " + std::string(sourceRelativePath);
    } else {
      error = "无法解析资源路径: " + std::string(sourceRelativePath);
    }
    return false;
  }
  if (!pathWithin(root, canonical)) {
    error = "解析后的路径越出授权根目录";
    return false;
  }
  return readFile(canonical, kMaxPngBytes, bytes, error);
}

DirectoryResourceStaging::DirectoryResourceStaging(std::string projectRoot) {
  std::error_code ec;
  const fs::path canonical = fs::canonical(fs::path(projectRoot), ec);
  if (ec || !fs::is_directory(canonical, ec)) return;
  root_ = canonical.string();
  valid_ = true;
}

DirectoryResourceStaging::~DirectoryResourceStaging() {
  if (!completed_) rollback();
}

bool DirectoryResourceStaging::stage(std::string_view projectRelativePath,
                                     const std::vector<std::uint8_t>& bytes,
                                     std::string& error) {
  if (!valid_ || completed_) {
    error = "资源暂存事务已结束";
    return false;
  }
  const std::string relative(projectRelativePath);
  if (!isSafeRelative(relative) || relative.find('\\') != std::string::npos) {
    error = "目标路径必须是授权根目录内的相对 POSIX 路径";
    return false;
  }
  const fs::path rel(relative);
  if (rel.begin() == rel.end() || *rel.begin() != "assets") {
    error = "目标路径必须位于工程 assets 目录下";
    return false;
  }
  const fs::path target = fs::path(root_) / rel;
  std::error_code ec;
  fs::create_directories(target.parent_path(), ec);
  if (ec) {
    error = "无法创建目标目录";
    return false;
  }
  const fs::path canonicalParent = fs::canonical(target.parent_path(), ec);
  if (ec || !pathWithin(fs::path(root_), canonicalParent)) {
    error = "目标目录越出授权根目录";
    return false;
  }
  if (fs::exists(target, ec)) {
    error = "目标文件已存在，拒绝覆盖";
    return false;
  }
  const fs::path temporary = target.string() + ".stage-" + Id::generate(IdKind::Project).str();
  std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "无法创建临时文件";
    return false;
  }
  if (!bytes.empty()) {
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }
  out.flush();
  if (!out) {
    out.close();
    fs::remove(temporary, ec);
    error = "临时文件写入失败";
    return false;
  }
  out.close();
  std::vector<std::uint8_t> readback;
  if (!readFile(temporary, kMaxPngBytes, readback, error) || readback != bytes) {
    fs::remove(temporary, ec);
    error = "临时文件写入后大小不符";
    return false;
  }
  entries_.push_back({temporary.string(), target.string(), false});
  return true;
}

bool DirectoryResourceStaging::publish(std::string& error) {
  if (!valid_ || completed_) {
    error = "资源暂存事务已结束";
    return false;
  }
  for (Entry& entry : entries_) {
    std::error_code ec;
#ifdef _WIN32
    if (::MoveFileExW(fs::path(entry.temporary).c_str(), fs::path(entry.target).c_str(),
                      MOVEFILE_WRITE_THROUGH) == 0) {
      error = "临时资源移动失败";
      rollback();
      return false;
    }
#else
    if (::link(entry.temporary.c_str(), entry.target.c_str()) != 0) {
      error = "资源发布失败";
      rollback();
      return false;
    }
    fs::remove(entry.temporary, ec);
#endif
    entry.published = true;
  }
  return true;
}

void DirectoryResourceStaging::rollback() noexcept {
  std::error_code ec;
  for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
    fs::remove(it->temporary, ec);
    ec.clear();
    if (it->published) {
      fs::remove(it->target, ec);
      ec.clear();
    }
  }
  entries_.clear();
}

void DirectoryResourceStaging::complete() noexcept {
  std::error_code ec;
  for (const Entry& entry : entries_) fs::remove(entry.temporary, ec);
  entries_.clear();
  completed_ = true;
}

const char* toString(AtlasImportCode code) noexcept {
  switch (code) {
    case AtlasImportCode::kNone: return "ATLAS_IMPORT_NONE";
    case AtlasImportCode::kAtlasSyntax: return "ATLAS_IMPORT_SYNTAX";
    case AtlasImportCode::kAtlasValueInvalid: return "ATLAS_IMPORT_VALUE_INVALID";
    case AtlasImportCode::kAtlasFeatureUnsupported: return "ATLAS_IMPORT_FEATURE_UNSUPPORTED";
    case AtlasImportCode::kSourceResourceMissing: return "ATLAS_IMPORT_RESOURCE_MISSING";
    case AtlasImportCode::kSourcePathRejected: return "ATLAS_IMPORT_PATH_REJECTED";
    case AtlasImportCode::kPngInvalid: return "ATLAS_IMPORT_PNG_INVALID";
    case AtlasImportCode::kDomainRejected: return "ATLAS_IMPORT_DOMAIN_REJECTED";
    case AtlasImportCode::kWarningsNotAccepted: return "ATLAS_IMPORT_WARNINGS_NOT_ACCEPTED";
    case AtlasImportCode::kResourceStageFailed: return "ATLAS_IMPORT_STAGE_FAILED";
    case AtlasImportCode::kProjectSaveFailed: return "ATLAS_IMPORT_SAVE_FAILED";
  }
  return "ATLAS_IMPORT_UNKNOWN";
}

// ---- Task 6 B1：目录列举（文件系统实现仍在**插件侧**，接口在 importers/api）----
bool FileSystemResourceResolver::listDirectory(std::string_view sourceDirectory,
                                               std::vector<std::string>& names,
                                               std::string& error) const {
  names.clear();
  if (!valid_) {
    error = "授权根目录无效";
    return false;
  }
  const std::string relative(sourceDirectory);
  const std::string target = relative.empty() ? std::string(".") : relative;
  if (!isSafeRelative(target)) {
    error = "路径必须是授权根目录内的相对路径";
    return false;
  }
  const fs::path root(root_);
  const fs::path dir = root / fs::path(normalizedRelative(target));
  std::error_code ec;
  const fs::path canonical = fs::canonical(dir, ec);
  if (ec) {
    // 与 read() 同款区分：「目录不存在」和「无法解析路径」语义不同，不得混为一谈
    std::error_code existsEc;
    if (!fs::exists(dir, existsEc)) {
      error = "目录不存在: " + relative;
    } else {
      error = "无法解析目录路径: " + relative;
    }
    return false;
  }
  if (!pathWithin(root, canonical)) {
    error = "解析后的路径越出授权根目录";
    return false;
  }
  if (!fs::is_directory(canonical, ec)) {
    error = "不是目录: " + relative;
    return false;
  }
  for (const auto& entry : fs::directory_iterator(canonical, ec)) {
    if (ec) {
      error = "目录不可读: " + relative;
      return false;
    }
    // 只列**一层普通文件**的文件名：不递归、不含子目录、不含路径
    std::error_code fileEc;
    if (entry.is_regular_file(fileEc)) names.push_back(entry.path().filename().string());
  }
  std::sort(names.begin(), names.end());  // 确定性排序：宿主实现差异不得改变插件结果
  return true;
}

}  // namespace loongbones::importers::spine32
