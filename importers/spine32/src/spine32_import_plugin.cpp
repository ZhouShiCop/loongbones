// Task 4 B6: Spine 3.2.01 插件的 prepare() —— 走完「二进制 → 图集 → 映射 → 域校验」。
//
// 插件的**唯一**产出入口：只读源、产出 ImportDraft。
// 不写盘、不改 request.current、不调用任何 Project 写 API（B6 第 2/3 条）。
//
// 一个「单位」= `.skel.bytes` + 同名 `.atlas.txt` + 图集页引用的 PNG。

#include "loongbones/importers/spine32/spine32_import_plugin.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "loongbones/domain/atlas.hpp"
#include "loongbones/domain/ids.hpp"
#include "loongbones/domain/project.hpp"
#include "loongbones/domain/skeleton.hpp"
#include "loongbones/format/project_document.hpp"
#include "loongbones/importers/api/path_util.hpp"
#include "loongbones/importers/api/sha256.hpp"
#include "loongbones/importers/spine32/atlas_import.hpp"
#include "loongbones/importers/spine32/atlas_text_reader.hpp"
#include "loongbones/importers/spine32/binary_reader.hpp"
#include "loongbones/importers/spine32/mapper.hpp"
#include "loongbones/importers/spine32/source_model.hpp"

namespace loongbones::importers::spine32 {
namespace {

constexpr std::string_view kSkelSuffix = ".skel.bytes";

/// `h3181c2/h3181c2.skel.bytes` → `h3181c2/h3181c2.atlas.txt`
std::string atlasPathFor(std::string_view skeletonPath) {
  const std::string path(skeletonPath);
  if (path.size() >= kSkelSuffix.size() &&
      path.compare(path.size() - kSkelSuffix.size(), kSkelSuffix.size(), kSkelSuffix) == 0) {
    return path.substr(0, path.size() - kSkelSuffix.size()) + ".atlas.txt";
  }
  return path + ".atlas.txt";
}

std::vector<std::string> splitSegments(std::string_view text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (true) {
    const auto slash = text.find('/', start);
    out.emplace_back(text.substr(
        start, slash == std::string_view::npos ? std::string_view::npos : slash - start));
    if (slash == std::string_view::npos) break;
    start = slash + 1;
  }
  return out;
}

/// 把 `name` 相对 `basePath` 的**目录**解析成授权根相对路径。越出根部 → 空串。
std::string siblingPath(std::string_view basePath, std::string_view name) {
  const std::string base(basePath);
  const auto slash = base.find_last_of('/');
  const std::string dir =
      (slash == std::string_view::npos) ? std::string{} : base.substr(0, slash);

  std::vector<std::string> parts;
  bool escaped = false;
  for (std::string_view text : {std::string_view(dir), name}) {
    for (const std::string& seg : splitSegments(text)) {
      if (seg.empty() || seg == ".") continue;
      if (seg == "..") {
        if (parts.empty()) {
          escaped = true;
          break;
        }
        parts.pop_back();
        continue;
      }
      parts.push_back(seg);
    }
    if (escaped) break;
  }
  if (escaped) return {};

  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i != 0) out += '/';
    out += parts[i];
  }
  return out;
}

/// 在 used 集合里为 base 取一个不重复的名字（稳定可复现）。
std::string uniqueName(std::string base, std::set<std::string>& used) {
  if (base.empty()) base = "unnamed";
  std::string candidate = base;
  int suffix = 1;
  while (used.find(candidate) != used.end()) {
    candidate = base + "_" + std::to_string(suffix);
    ++suffix;
  }
  used.insert(candidate);
  return candidate;
}

std::string stemOf(std::string_view path) {
  const std::string p(path);
  const auto slash = p.find_last_of('/');
  const std::string name = (slash == std::string::npos) ? p : p.substr(slash + 1);
  if (name.size() >= kSkelSuffix.size() &&
      name.compare(name.size() - kSkelSuffix.size(), kSkelSuffix.size(), kSkelSuffix) == 0) {
    return name.substr(0, name.size() - kSkelSuffix.size());
  }
  const auto dot = name.find('.');
  return (dot == std::string::npos) ? name : name.substr(0, dot);
}

api::ImportCode toImportCode(AtlasTextCode code) noexcept {
  switch (code) {
    case AtlasTextCode::kNone: return api::ImportCode::kNone;
    // 「页/区域属性不进当前 Domain 模型」是**能力缺口**，不是解析失败。
    // 曾把所有非 kNone 一律映射成 kParseError，于是语料里 3333 条
    // 「页属性未保存」告警被计成解析错误（B6 复核发现项，2026-10-21）。
    case AtlasTextCode::kFeatureUnsupported: return api::ImportCode::kUnsupportedCapability;
    case AtlasTextCode::kSyntax:
    case AtlasTextCode::kValueInvalid: break;
  }
  return api::ImportCode::kParseError;
}

api::ImportCode toImportCode(MapperCode code) noexcept {
  switch (code) {
    case MapperCode::kAttachmentRegionUnresolved: return api::ImportCode::kUnresolvedReference;
    case MapperCode::kAttachmentRegionDegraded: return api::ImportCode::kAttachmentRegionDegraded;
    case MapperCode::kUnsupportedCapability: return api::ImportCode::kUnsupportedCapability;
    case MapperCode::kNone: return api::ImportCode::kNone;
    case MapperCode::kUnsupportedVersion:
    case MapperCode::kBoneParentOutOfRange:
    case MapperCode::kSlotBoneOutOfRange:
    case MapperCode::kSkinSlotOutOfRange:
    case MapperCode::kNonFiniteFloat:
      break;
  }
  return api::ImportCode::kParseError;
}

api::ImportSeverity toImportSeverity(MapperSeverity severity) noexcept {
  return severity == MapperSeverity::kWarning ? api::ImportSeverity::kWarning
                                              : api::ImportSeverity::kError;
}

}  // namespace

const api::ImportDescriptor& Spine32ImportPlugin::descriptor() const noexcept {
  static const api::ImportDescriptor descriptor{kSpine32PluginId, kSpine32FormatId,
                                                kSpine32FormatVersion};
  return descriptor;
}

api::ImportDraft Spine32ImportPlugin::prepare(const api::ImportRequest& request) const {
  api::ImportDraft draft;
  draft.descriptor = descriptor();

  auto addDiag = [&draft](api::ImportCode code, api::ImportSeverity severity,
                          std::string message, std::string location) {
    draft.report.diagnostics.push_back({code, severity, std::move(message), std::move(location)});
  };

  // ---- 0) 源路径必须落在授权根内（词法检查，与宿主 Resolver 同一判定） ----
  if (!api::isSafeRelativePath(request.sourceLogicalPath)) {
    addDiag(api::ImportCode::kSourceRejected, api::ImportSeverity::kError,
            "源路径必须是授权根目录内的相对路径", request.sourceLogicalPath);
    return draft;
  }
  if (request.cancelled()) {
    addDiag(api::ImportCode::kCancelled, api::ImportSeverity::kError, "导入已被调用方取消",
            request.sourceLogicalPath);
    return draft;
  }

  // ---- 1) 读 `.skel.bytes` ----
  request.progress("read-skeleton", 0, 5);
  std::vector<std::uint8_t> skeletonBytes;
  std::string error;
  if (!request.resolver.read(request.sourceLogicalPath, skeletonBytes, error)) {
    addDiag(api::ImportCode::kSourceMissing, api::ImportSeverity::kError,
            "无法读取骨架源文件: " + error, request.sourceLogicalPath);
    return draft;
  }
  if (request.cancelled()) {
    addDiag(api::ImportCode::kCancelled, api::ImportSeverity::kError, "导入已被调用方取消",
            request.sourceLogicalPath);
    return draft;
  }

  // ---- 2) 解析二进制 → SourceSkeleton ----
  request.progress("parse-binary", 1, 5);
  const ReadResult read = readSkeletonBinary32(skeletonBytes);
  for (const ReadDiagnostic& d : read.diagnostics) {
    addDiag(api::ImportCode::kParseError, api::ImportSeverity::kError, d.message,
            d.section + "@" + std::to_string(d.offset));
  }
  if (!read.ok) return draft;
  if (request.cancelled()) {
    addDiag(api::ImportCode::kCancelled, api::ImportSeverity::kError, "导入已被调用方取消",
            request.sourceLogicalPath);
    return draft;
  }

  // ---- 3) 读同名 `.atlas.txt` ----
  request.progress("read-atlas", 2, 5);
  const std::string atlasPath = atlasPathFor(request.sourceLogicalPath);
  std::vector<std::uint8_t> atlasBytes;
  if (!request.resolver.read(atlasPath, atlasBytes, error)) {
    addDiag(api::ImportCode::kResourceMissing, api::ImportSeverity::kError,
            "无法读取图集文件: " + error, atlasPath);
    return draft;
  }
  const AtlasText atlasText =
      readAtlasText(std::string(atlasBytes.begin(), atlasBytes.end()));
  for (const AtlasTextDiagnostic& d : atlasText.diagnostics) {
    addDiag(toImportCode(d.code),
            d.severity == AtlasTextSeverity::kWarning ? api::ImportSeverity::kWarning
                                                      : api::ImportSeverity::kError,
            d.message, d.sourceLocation);
  }
  if (!atlasText.valid) return draft;

  // ---- 4) 逐页解析 PNG：只取 IHDR 像素尺寸，不解码像素 ----
  request.progress("inspect-png", 3, 5);
  Atlas atlas;
  atlas.id = request.ids.create(IdKind::Atlas);
  atlas.name = stemOf(request.sourceLogicalPath);

  std::vector<TextureAsset> textures;
  std::vector<api::StagedResource> resources;
  std::size_t pagePropertyCount = 0;

  // 纹理资产名字在工程内必须唯一；多次导入同一工程时页文件名可能撞车，
  // 因此以**现有**名字集合为基数做去重命名（与 B1 同名辅助函数同一策略）。
  std::set<std::string> usedTextureNames;
  for (const TextureAsset& existing : request.current.data.textures) {
    usedTextureNames.insert(existing.name);
  }
  for (const SourceAtlasPage& srcPage : atlasText.pages) {
    const std::string pagePath = siblingPath(atlasPath, srcPage.file);
    if (pagePath.empty()) {
      addDiag(api::ImportCode::kSourceRejected, api::ImportSeverity::kError,
              "图集页路径越出授权根目录", srcPage.file);
      return draft;
    }
    std::vector<std::uint8_t> pngBytes;
    if (!request.resolver.read(pagePath, pngBytes, error)) {
      addDiag(api::ImportCode::kResourceMissing, api::ImportSeverity::kError,
              "无法读取图集页资源: " + error, pagePath);
      return draft;
    }
    PngInfo info;
    if (!inspectPng(pngBytes, info, error)) {
      addDiag(api::ImportCode::kResourceInvalid, api::ImportSeverity::kError,
              "PNG 校验失败: " + error, pagePath);
      return draft;
    }

    TextureAsset texture;
    texture.id = request.ids.create(IdKind::TextureAsset);
    texture.name = uniqueName(stemOf(srcPage.file), usedTextureNames);
    texture.location = "assets/textures/" + texture.id.str() + ".png";
    texture.pixelWidth = info.width;
    texture.pixelHeight = info.height;
    textures.push_back(texture);
    // PNG 全文进暂存清单：提交阶段由宿主写进 `texture.location` 指向的工程内路径。
    // 不在这里写盘（B6 第 2 条：prepare 只产出候选）。
    resources.push_back(api::StagedResource{texture.location, std::move(pngBytes)});

    AtlasPage page;
    page.id = request.ids.create(IdKind::AtlasPage);
    page.name = srcPage.file;
    page.textureAssetId = texture.id;
    // 页**声明**坐标尺寸，与 PNG 像素尺寸分别保存（B5 验收项）。
    page.width = srcPage.width;
    page.height = srcPage.height;
    atlas.pages.push_back(page);

    for (const SourceAtlasRegion& src : srcPage.regions) {
      AtlasRegion region;
      region.id = request.ids.create(IdKind::AtlasRegion);
      region.name = src.name;
      region.pageId = page.id;
      region.packedX = src.x;
      region.packedY = src.y;
      region.packedW = src.width;
      region.packedH = src.height;
      region.originalW = src.originalWidth;
      region.originalH = src.originalHeight;
      region.trimmedW = src.trimmedWidth;
      region.trimmedH = src.trimmedHeight;
      region.offsetX = src.offsetX;
      region.offsetY = src.offsetY;
      region.rotation = src.rotation;
      atlas.regions.push_back(std::move(region));
    }

    if (srcPage.properties.any()) ++pagePropertyCount;
  }
  if (pagePropertyCount != 0) {
    draft.report.unsupported.emplace_back("atlasPageProperties", pagePropertyCount);
  }

  // ---- 5) Source → canonical 映射（含 region 查找） ----
  request.progress("map-cannonical", 4, 5);
  AtlasRegionIndex index({atlas});
  MapperOptions mapperOptions;
  mapperOptions.skeletonName = atlas.name;
  mapperOptions.atlasIds = {atlas.id};
  SourceRef ref;
  ref.formatId = kSpine32FormatId;
  ref.formatVersion = kSpine32FormatVersion;
  ref.sourceHeaderHash = read.skeleton.hash;
  // contentHash = 宿主/插件对 `.skel.bytes` 全文算的 SHA-256（D25）。
  ref.contentHash = api::sha256Hex(skeletonBytes);
  ref.sourceUnit = request.sourceLogicalPath;
  // Domain 校验把「谁导入的」列为 sourceRef 的必需字段（kSkeletonSourceRefInvalid），
  // 缺省会连累整份候选被 adoptData() 拒绝。
  ref.importedByPluginId = kSpine32PluginId;
  ref.importedByPluginVersion = kSpine32PluginVersion;
  mapperOptions.sourceRef = ref;

  const MapResult mapped = mapSkeletonToCanonical(read.skeleton, mapperOptions, index, request.ids);
  for (const MapperDiagnostic& d : mapped.diagnostics) {
    addDiag(toImportCode(d.code), toImportSeverity(d.severity), d.message, d.sourceLocation);
  }

  const UnsupportedCounts& u = mapped.unsupported;
  auto pushUnsupported = [&draft](const char* name, std::size_t count) {
    if (count != 0) draft.report.unsupported.emplace_back(name, count);
  };
  pushUnsupported("mesh", u.mesh);
  pushUnsupported("linkedMesh", u.linkedMesh);
  pushUnsupported("weightedMesh", u.weightedMesh);
  pushUnsupported("weightedLinkedMesh", u.weightedLinkedMesh);
  pushUnsupported("boundingBox", u.boundingBox);
  pushUnsupported("otherAttachmentType", u.otherAttachmentType);
  pushUnsupported("ikConstraints", u.ikConstraints);
  pushUnsupported("transformConstraints", u.transformConstraints);
  pushUnsupported("events", u.events);
  pushUnsupported("animations", u.animations);
  pushUnsupported("timelines", u.timelines);
  pushUnsupported("deformTimelines", u.deformTimelines);

  if (!mapped.valid) return draft;

  // ---- 6) 完整候选副本 → 临时 Project::adoptData() 全量校验 ----
  ProjectData candidate = request.current.data;
  for (const TextureAsset& texture : textures) candidate.textures.push_back(texture);
  candidate.atlases.push_back(atlas);
  candidate.skeletons.push_back(mapped.skeleton);

  Project probe = Project::createWithId(candidate.id, candidate.name);
  const Status status = probe.adoptData(candidate);
  if (!status.ok()) {
    addDiag(api::ImportCode::kDomainRejected, api::ImportSeverity::kError,
            "候选工程未通过 Domain 校验: " + status.diagnostic().message,
            status.diagnostic().location);
    return draft;
  }

  format::ProjectDocument document = request.current;
  document.data = std::move(candidate);
  document.sourceVersion = format::SourceFormatVersion::kV2;

  std::size_t skinEntryCount = 0;
  for (const Skin& skin : mapped.skeleton.skins) skinEntryCount += skin.entries.size();

  draft.document = std::move(document);
  draft.resources = std::move(resources);
  draft.report.counts = {{"skeleton", 1},          {"atlas", 1},
                        {"texture", textures.size()}, {"atlasPage", atlas.pages.size()},
                        {"atlasRegion", atlas.regions.size()}, {"bone", mapped.skeleton.bones.size()},
                        {"slot", mapped.skeleton.slots.size()}, {"skin", mapped.skeleton.skins.size()},
                        {"skinEntry", skinEntryCount}, {"regionAttachment", mapped.regionAttachmentCount}};
  draft.valid = true;
  return draft;
}

}  // namespace loongbones::importers::spine32
