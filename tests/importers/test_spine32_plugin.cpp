// Task 4 B6：Spine 3.2.01 插件的端到端断言（prepare → 校验 → 提交）。
//
// 覆盖 B6 第 1/2 条与 field-mapping §10.1：
//   · descriptor 精确匹配 `spine.binary@3.2.01`；
//   · 一个单位 = `.skel.bytes` + 同名 `.atlas.txt` + 页引用 PNG；
//   · 附件 region 查找键 `path ?? name`，降级给 warning，未命中是 error；
//   · 不支持的能力只计数 + warning，不建半成品 Domain 对象。
//
// 输入全部是内存合成 fixture，不读真实语料。

#include "loongbones/importers/spine32/spine32_import_plugin.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "loongbones/domain/atlas.hpp"
#include "loongbones/domain/ids.hpp"
#include "loongbones/domain/project.hpp"
#include "loongbones/domain/skeleton.hpp"
#include "loongbones/format/format_diagnostics.hpp"
#include "loongbones/format/project_io.hpp"
#include "loongbones/importers/api/import_plugin.hpp"
#include "loongbones/importers/api/sha256.hpp"
#include "loongbones/importers/host/import_host.hpp"
#include "loongbones/importers/spine32/atlas_import.hpp"
#include "loongbones/importers/spine32/binary_reader.hpp"
#include "tests/domain/test_framework.hpp"

namespace fs = std::filesystem;

namespace {

using namespace loongbones;
using namespace loongbones::importers;

// ---- 合成字节写入器（大端，与源格式一致）----
struct W {
  std::vector<std::uint8_t> o;
  void raw(std::uint8_t v) { o.push_back(v); }
  void u8(std::uint8_t v) { o.push_back(v); }
  void boolv(bool v) { o.push_back(v ? 1 : 0); }
  void varint(std::uint32_t v) {
    do {
      std::uint8_t b = static_cast<std::uint8_t>(v & 0x7Fu);
      v >>= 7;
      if (v != 0u) b |= 0x80u;
      o.push_back(b);
    } while (v != 0u);
  }
  void str(const std::string& s) {
    varint(static_cast<std::uint32_t>(s.size() + 1));
    for (char c : s) o.push_back(static_cast<std::uint8_t>(c));
  }
  void strNull() { o.push_back(0); }
  void f32(float v) {
    std::uint32_t raw = 0;
    std::memcpy(&raw, &v, sizeof(raw));
    o.push_back(static_cast<std::uint8_t>(raw >> 24));
    o.push_back(static_cast<std::uint8_t>(raw >> 16));
    o.push_back(static_cast<std::uint8_t>(raw >> 8));
    o.push_back(static_cast<std::uint8_t>(raw));
  }
  void i32(std::int32_t v) {
    const std::uint32_t raw = static_cast<std::uint32_t>(v);
    o.push_back(static_cast<std::uint8_t>(raw >> 24));
    o.push_back(static_cast<std::uint8_t>(raw >> 16));
    o.push_back(static_cast<std::uint8_t>(raw >> 8));
    o.push_back(static_cast<std::uint8_t>(raw));
  }
  std::vector<std::uint8_t> bytes() const { return o; }
};

std::uint32_t crc32Of(const std::uint8_t* data, std::size_t size) {
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < size; ++i) {
    crc ^= static_cast<std::uint32_t>(data[i]);
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = static_cast<std::uint32_t>(-(static_cast<std::int32_t>(crc & 1u)));
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return ~crc;
}

void pushBe32(std::vector<std::uint8_t>& b, std::uint32_t v) {
  b.push_back(static_cast<std::uint8_t>(v >> 24));
  b.push_back(static_cast<std::uint8_t>(v >> 16));
  b.push_back(static_cast<std::uint8_t>(v >> 8));
  b.push_back(static_cast<std::uint8_t>(v));
}

/// 最小合法 PNG：signature + IHDR（含 CRC）。**不含像素数据** ——
/// 本阶段只取 IHDR 尺寸，不解码像素。
std::vector<std::uint8_t> makePng(std::uint32_t width, std::uint32_t height) {
  const std::uint8_t signature[8] = {0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au};
  std::vector<std::uint8_t> b;
  for (std::uint8_t v : signature) b.push_back(v);
  pushBe32(b, 13);
  const std::size_t typeAt = b.size();
  b.push_back('I'); b.push_back('H'); b.push_back('D'); b.push_back('R');
  pushBe32(b, width);
  pushBe32(b, height);
  b.push_back(8);  // bit depth
  b.push_back(6);  // color type RGBA
  b.push_back(0); b.push_back(0); b.push_back(0);
  pushBe32(b, crc32Of(b.data() + typeAt, 17));
  return b;
}

/// 造一份只含**一个 region** 的图集文本；region 名由调用方指定，
/// 以便分别构造「精确命中 / 尾随空格降级 / 未命中」三种查找情形。
std::string makeAtlas(const std::string& pageFile, const std::string& regionName, int width,
                      int height) {
  std::string out = pageFile + "\n";
  out += "size: " + std::to_string(width) + "," + std::to_string(height) + "\n";
  out += "\n";
  out += regionName + "\n";
  out += "  xy: 0,0\n";
  out += "  size: 10,20\n";
  return out;
}

/// 在页头追加一条属性（如 `filter`）。用于构造「当前 Domain 不承载的页属性」。
std::string makeAtlasWithPageProperty(const std::string& pageFile, const std::string& regionName,
                                      int width, int height, const std::string& property) {
  std::string out = pageFile + "\n";
  out += "size: " + std::to_string(width) + "," + std::to_string(height) + "\n";
  out += property + "\n";
  out += "\n";
  out += regionName + "\n";
  out += "  xy: 0,0\n";
  out += "  size: 10,20\n";
  return out;
}

// ---- 最小骨架二进制（1 骨 / 0 ik / 0 transform / 1 槽 / 1 附件）----
void writeHeader(W& w, bool nonessential, const std::string& version = "3.2.01") {
  w.str("h");
  w.str(version);
  w.f32(1.f);
  w.f32(1.f);
  w.boolv(nonessential);
  if (nonessential) w.str("images/");
}

void writeBone(W& w, const std::string& name, bool nonessential, bool indexed) {
  w.str(name);
  if (indexed) w.varint(0);
  w.f32(0.f); w.f32(0.f); w.f32(0.f);  // rotation, x, y
  w.f32(1.f); w.f32(1.f);              // scaleX, scaleY
  w.f32(0.f); w.f32(0.f); w.f32(0.f);  // shearX, shearY, length
  w.boolv(true);
  w.boolv(true);
  if (nonessential) w.i32(-1);
}

/// region 附件载荷：path(strNull) + 7×f32 + color(i32)
void writeRegionPayload(W& w) {
  w.strNull();
  w.f32(0.f); w.f32(0.f); w.f32(0.f);  // rotation, x, y
  w.f32(1.f); w.f32(1.f);              // scaleX, scaleY
  w.f32(10.f); w.f32(20.f);             // width, height
  w.i32(-1);
}

/// 拼一份最小骨架：default 皮肤里一个名为 `attachmentName` 的 region 附件。
std::vector<std::uint8_t> minimalSkeleton(const std::string& attachmentName) {
  W w;
  writeHeader(w, false);
  w.varint(1);                       // bones
  writeBone(w, "root", false, false);
  w.varint(0); w.varint(0);          // ik, transform
  w.varint(1);                       // slots
  w.str("s");
  w.varint(0);
  w.i32(-1);
  w.strNull();
  w.u8(0);
  w.varint(1);                       // default skin: 1 slot
  w.varint(0);                       // slotIndex
  w.varint(1);                       // 1 attachment
  w.str(attachmentName);
  w.strNull();
  w.u8(0);                           // region
  writeRegionPayload(w);
  w.varint(0);                       // extra skins
  w.varint(0);                       // events
  w.varint(0);                       // animations
  return w.bytes();
}

/// 授权根内的内存源。
class FakeResolver final : public api::IResourceResolver {
 public:
  void add(std::string path, std::vector<std::uint8_t> bytes) {
    files_[std::move(path)] = std::move(bytes);
  }
  bool read(std::string_view path, std::vector<std::uint8_t>& bytes,
            std::string& error) const override {
    const auto it = files_.find(std::string(path));
    if (it == files_.end()) {
      error = "no such resource";
      return false;
    }
    bytes = it->second;
    return true;
  }

 private:
  std::map<std::string, std::vector<std::uint8_t>> files_;
};

class RecordingProgress final : public api::IProgressSink {
 public:
  void report(std::string_view stage, std::size_t done, std::size_t total) override {
    stages_.emplace_back(stage);
    lastDone_ = done;
    lastTotal_ = total;
  }
  std::size_t lastDone() const { return lastDone_; }
  const std::vector<std::string>& stages() const { return stages_; }
  std::size_t lastTotal() const { return lastTotal_; }

 private:
  std::vector<std::string> stages_;
  std::size_t lastDone_ = 0;
  std::size_t lastTotal_ = 0;
};

class ManualCancel final : public api::ICancelToken {
 public:
  bool isCancelled() const noexcept override { return cancelled_; }
  void set(bool value) noexcept { cancelled_ = value; }

 private:
  bool cancelled_ = false;
};

bool hasCode(const std::vector<api::ImportDiagnostic>& diags, api::ImportCode code) {
  for (const api::ImportDiagnostic& d : diags) if (d.code == code) return true;
  return false;
}

/// 一份空的**格式层文档**（含 kRoot 场景与根节点）。
/// 注意：`ProjectData` 不会隐式转成 `ProjectDocument` —— 必须显式构造。
format::ProjectDocument emptyProject() { return format::createProjectDocument("p"); }

std::size_t countSeverity(const api::ImportReport& report, api::ImportSeverity severity) {
  std::size_t n = 0;
  for (const api::ImportDiagnostic& d : report.diagnostics) {
    if (d.severity == severity) ++n;
  }
  return n;
}

std::size_t countCode(const api::ImportReport& report, api::ImportCode code) {
  std::size_t n = 0;
  for (const api::ImportDiagnostic& d : report.diagnostics) {
    if (d.code == code) ++n;
  }
  return n;
}

/// 「不得静默丢失」的账本：某种不支持能力被计了多少次。
std::size_t unsupportedCount(const api::ImportReport& report, const std::string& name) {
  for (const auto& entry : report.unsupported) {
    if (entry.first == name) return entry.second;
  }
  return 0;
}

api::ImportDescriptor spine32Descriptor() {
  const spine32::Spine32ImportPlugin plugin;
  return plugin.descriptor();
}

/// 走一遍 prepare：源由内存提供，工程为新建空工程。
api::ImportDraft prepareUnit(const std::vector<std::uint8_t>& skeletonBytes,
                             const std::string& atlasText, bool withPng,
                             api::IIdFactory& ids,
                             const api::ICancelToken* cancel = nullptr,
                             api::IProgressSink* progress = nullptr) {
  FakeResolver resolver;
  resolver.add("unit/unit.skel.bytes", skeletonBytes);
  if (!atlasText.empty()) resolver.add("unit/unit.atlas.txt", std::vector<std::uint8_t>(atlasText.begin(), atlasText.end()));
  if (withPng) resolver.add("unit/unit.png", makePng(64, 64));

  api::ImportRequest request{emptyProject(), "unit/unit.skel.bytes", resolver, ids};
  request.cancel = cancel;
  request.progressSink = progress;

  const spine32::Spine32ImportPlugin plugin;
  return plugin.prepare(request);
}

}  // namespace

// ---- descriptor / 注册 ----

TEST(Spine32Plugin, DescriptorIsSpineBinary3201) {
  const api::ImportDescriptor d = spine32Descriptor();
  CHECK_EQ(d.pluginId, std::string{"loongbones.spine32"});
  CHECK_EQ(d.formatId, std::string{"spine.binary"});
  CHECK_EQ(d.formatVersion, std::string{"3.2.01"});
}

TEST(Spine32Plugin, CanImportMatchesExactly) {
  const spine32::Spine32ImportPlugin plugin;
  CHECK(plugin.canImport("spine.binary", "3.2.01"));
  CHECK(!plugin.canImport("spine.binary", "3.8.99"));
  CHECK(!plugin.canImport("spine.json", "3.2.01"));
}

TEST(Spine32Plugin, RegisterBuiltinPluginsMakesItFindable) {
  auto& reg = host::ImportRegistry::instance();
  reg.clear();
  host::registerBuiltinPlugins(reg);
  const api::IImportPlugin* p = reg.find("spine.binary", "3.2.01");
  CHECK(p != nullptr);
  CHECK(p->canImport("spine.binary", "3.2.01"));
  reg.clear();
}

// ---- prepare 的核心契约 ----

TEST(Spine32Plugin, MinimalUnitPreparesValidDraft) {
  spine32::RandomIdFactory ids;
  const auto draft = prepareUnit(minimalSkeleton("m"), makeAtlas("unit.png", "m", 64, 64), true, ids);

  CHECK(draft.valid);
  CHECK_EQ(draft.report.errorCount(), std::size_t{0});
  CHECK_EQ(draft.document.data.skeletons.size(), std::size_t{1});
  CHECK_EQ(draft.document.data.atlases.size(), std::size_t{1});
  CHECK_EQ(draft.document.data.textures.size(), std::size_t{1});
  CHECK_EQ(draft.document.data.atlases[0].pages.size(), std::size_t{1});
  CHECK_EQ(draft.document.data.atlases[0].regions.size(), std::size_t{1});
  CHECK_EQ(draft.resources.size(), std::size_t{1});
  CHECK_EQ(draft.resources[0].projectLocation,
          draft.document.data.textures[0].location);
  CHECK_EQ(static_cast<int>(draft.document.sourceVersion),
          static_cast<int>(format::SourceFormatVersion::kV2));
}

TEST(Spine32Plugin, PrepareDoesNotMutateCurrent) {
  FakeResolver resolver;
  resolver.add("unit/unit.skel.bytes", minimalSkeleton("m"));
  const std::string atlasText = makeAtlas("unit.png", "m", 64, 64);
  resolver.add("unit/unit.atlas.txt",
               std::vector<std::uint8_t>(atlasText.begin(), atlasText.end()));
  resolver.add("unit/unit.png", makePng(64, 64));

  spine32::RandomIdFactory ids;
  api::ImportRequest request{emptyProject(), "unit/unit.skel.bytes", resolver, ids};

  // 提交前的**对象计数快照**；prepare 之后逐项比对。
  struct Counts {
    std::size_t scenes;
    std::size_t nodes;
    std::size_t textures;
    std::size_t atlases;
    std::size_t skeletons;
  };
  const Counts before{request.current.data.scenes.size(), request.current.data.nodes.size(),
                      request.current.data.textures.size(), request.current.data.atlases.size(),
                      request.current.data.skeletons.size()};

  const spine32::Spine32ImportPlugin plugin;
  const api::ImportDraft draft = plugin.prepare(request);

  // prepare 成功 ≠ 目标工程被改动。current 必须逐项与快照一致，
  // 而 draft.document 才是被追加了对象的那一份。
  CHECK(draft.valid);
  CHECK_EQ(request.current.data.scenes.size(), before.scenes);
  CHECK_EQ(request.current.data.nodes.size(), before.nodes);
  CHECK_EQ(request.current.data.textures.size(), before.textures);
  CHECK_EQ(request.current.data.atlases.size(), before.atlases);
  CHECK_EQ(request.current.data.skeletons.size(), before.skeletons);

  CHECK_EQ(draft.document.data.textures.size(), std::size_t{1});
  CHECK_EQ(draft.document.data.skeletons.size(), std::size_t{1});
}

TEST(Spine32Plugin, AttachmentNameIsUsedWhenPathAbsent) {
  spine32::RandomIdFactory ids;
  const auto draft =
      prepareUnit(minimalSkeleton("head"), makeAtlas("unit.png", "head", 64, 64), true, ids);
  CHECK(draft.valid);
  const Skeleton& skeleton = draft.document.data.skeletons[0];
  CHECK_EQ(skeleton.skins.size(), std::size_t{1});
  CHECK_EQ(skeleton.skins[0].entries.size(), std::size_t{1});
  CHECK_EQ(skeleton.skins[0].entries[0].attachmentName, std::string{"head"});
  CHECK_EQ(draft.document.data.atlases[0].regions[0].name, std::string{"head"});
}

TEST(Spine32Plugin, TrailingSpaceDegradesToWarning) {
  spine32::RandomIdFactory ids;
  const auto draft =
      prepareUnit(minimalSkeleton("head "), makeAtlas("unit.png", "head", 64, 64), true, ids);
  CHECK(draft.valid);
  CHECK(hasCode(draft.report.diagnostics, api::ImportCode::kAttachmentRegionDegraded));
  CHECK_EQ(countSeverity(draft.report, api::ImportSeverity::kWarning), std::size_t{1});
}

TEST(Spine32Plugin, UnresolvedRegionIsAnError) {
  spine32::RandomIdFactory ids;
  const auto draft =
      prepareUnit(minimalSkeleton("nope"), makeAtlas("unit.png", "m", 64, 64), true, ids);
  CHECK(!draft.valid);
  CHECK(hasCode(draft.report.diagnostics, api::ImportCode::kUnresolvedReference));
  CHECK(draft.report.errorCount() >= 1);
}

// 「页属性不进 Domain」是**能力缺口**（warning），不是解析失败（error）。
// 曾因 AtlasTextCode → ImportCode 一律映射成 kParseError，语料里 3333 条
// 页属性告警被计成解析错误（B6 复核发现项，2026-10-21）。
TEST(Spine32Plugin, UnsupportedAtlasPagePropertyIsWarningNotParseError) {
  spine32::RandomIdFactory ids;
  const auto draft = prepareUnit(minimalSkeleton("m"),
                                 makeAtlasWithPageProperty("unit.png", "m", 64, 64,
                                                           "filter: Linear,Linear"),
                                 true, ids);
  CHECK(draft.valid);
  CHECK_EQ(countCode(draft.report, api::ImportCode::kUnsupportedCapability), std::size_t{1});
  CHECK_EQ(countCode(draft.report, api::ImportCode::kParseError), std::size_t{0});
  CHECK_EQ(draft.report.errorCount(), std::size_t{0});
  CHECK_EQ(countSeverity(draft.report, api::ImportSeverity::kWarning), std::size_t{1});
  // 降级为 warning 不等于消失：能力缺口仍必须进 unsupported 账本。
  CHECK_EQ(unsupportedCount(draft.report, "atlasPageProperties"), std::size_t{1});
}

// 反向守护：真正的图集**结构性**失败仍必须是 kParseError + error，
// 不能因为上面的修正被一并放宽成 warning。
TEST(Spine32Plugin, AtlasSyntaxErrorRemainsParseError) {
  spine32::RandomIdFactory ids;
  const std::string atlas = "unit.png\nsize: not,a-number\n\nm\n  xy: 0,0\n  size: 10,20\n";
  const auto draft = prepareUnit(minimalSkeleton("m"), atlas, true, ids);
  CHECK(!draft.valid);
  CHECK_EQ(countCode(draft.report, api::ImportCode::kParseError), std::size_t{1});
  CHECK_EQ(countCode(draft.report, api::ImportCode::kUnsupportedCapability), std::size_t{0});
  CHECK(draft.report.errorCount() >= 1);
}

TEST(Spine32Plugin, MissingAtlasIsReported) {
  spine32::RandomIdFactory ids;
  const auto draft = prepareUnit(minimalSkeleton("m"), std::string{}, true, ids);
  CHECK(!draft.valid);
  CHECK(hasCode(draft.report.diagnostics, api::ImportCode::kResourceMissing));
}

TEST(Spine32Plugin, MissingPngIsReported) {
  spine32::RandomIdFactory ids;
  const auto draft = prepareUnit(minimalSkeleton("m"), makeAtlas("unit.png", "m", 64, 64), false, ids);
  CHECK(!draft.valid);
  CHECK(hasCode(draft.report.diagnostics, api::ImportCode::kResourceMissing));
}

TEST(Spine32Plugin, InvalidPngIsReported) {
  const std::string atlasText = makeAtlas("unit.png", "m", 64, 64);
  FakeResolver resolver;
  resolver.add("unit/unit.skel.bytes", minimalSkeleton("m"));
  resolver.add("unit/unit.atlas.txt",
               std::vector<std::uint8_t>(atlasText.begin(), atlasText.end()));
  resolver.add("unit/unit.png", std::vector<std::uint8_t>{1, 2, 3, 4});

  spine32::RandomIdFactory ids;
  api::ImportRequest request{emptyProject(), "unit/unit.skel.bytes", resolver, ids};
  const spine32::Spine32ImportPlugin plugin;
  const auto draft = plugin.prepare(request);

  CHECK(!draft.valid);
  CHECK(hasCode(draft.report.diagnostics, api::ImportCode::kResourceInvalid));
}

TEST(Spine32Plugin, CancelledPrepareRejects) {
  spine32::RandomIdFactory ids;
  ManualCancel cancel;
  cancel.set(true);
  const auto draft =
      prepareUnit(minimalSkeleton("m"), makeAtlas("unit.png", "m", 64, 64), true, ids, &cancel);
  CHECK(!draft.valid);
  CHECK(hasCode(draft.report.diagnostics, api::ImportCode::kCancelled));
}

TEST(Spine32Plugin, SourceOutsideAuthorizedRootIsRejected) {
  FakeResolver resolver;
  spine32::RandomIdFactory ids;
  api::ImportRequest request{emptyProject(), "../escape.skel.bytes", resolver, ids};
  const spine32::Spine32ImportPlugin plugin;
  const auto draft = plugin.prepare(request);
  CHECK(!draft.valid);
  CHECK(hasCode(draft.report.diagnostics, api::ImportCode::kSourceRejected));
}

TEST(Spine32Plugin, ContentHashIsSha256OfSourceFile) {
  spine32::RandomIdFactory ids;
  const std::vector<std::uint8_t> bytes = minimalSkeleton("m");
  const auto draft = prepareUnit(bytes, makeAtlas("unit.png", "m", 64, 64), true, ids);
  CHECK(draft.valid);
  CHECK(draft.document.data.skeletons[0].sourceRef.has_value());
  CHECK_EQ(draft.document.data.skeletons[0].sourceRef->contentHash,
          api::sha256Hex(bytes));
}

TEST(Spine32Plugin, EndToEndCommitAndReopenMatch) {
  const fs::path dir = fs::temp_directory_path() / "lb_plugin_e2e";
  fs::remove_all(dir);
  fs::create_directories(dir);

  spine32::RandomIdFactory ids;
  const auto draft = prepareUnit(minimalSkeleton("m"), makeAtlas("unit.png", "m", 64, 64), true, ids);
  CHECK(draft.valid);

  spine32::DirectoryResourceStaging staging(dir.string());
  format::ProjectDocument doc = emptyProject();
  const auto outcome = host::commitImport(draft, {dir.string(), "dev", true}, staging, doc);
  CHECK(outcome.committed);

  // 资源文件已落盘，且路径就是纹理资产声明的位置。
  for (const api::StagedResource& r : draft.resources) {
    CHECK(fs::exists(dir / r.projectLocation));
  }
  CHECK(fs::exists(dir / format::kProjectFileName));

  // 重开工程：对象数必须与提交前一致（保存→重开的一致性）。
  const format::IoResult reopened = format::openProject(dir.string(), format::ReaderLimits{});
  if (!reopened.ok) {
    std::cout << "  reopen diag: " << reopened.formatDiag.str() << "\n";
    for (const Diagnostic& d : reopened.domainDiags) {
      std::cout << "    domain: [" << toString(d.code) << "] " << d.message << " @" << d.location
                << "\n";
    }
  }
  CHECK(reopened.ok);
  if (reopened.ok) {
    CHECK_EQ(reopened.document.data.skeletons.size(), std::size_t{1});
    CHECK_EQ(reopened.document.data.atlases.size(), std::size_t{1});
    CHECK_EQ(reopened.document.data.textures.size(), std::size_t{1});
    CHECK_EQ(reopened.document.data.atlases[0].regions.size(), std::size_t{1});
  }

  fs::remove_all(dir);
}
