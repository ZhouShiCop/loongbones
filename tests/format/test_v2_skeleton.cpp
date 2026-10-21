// 原生格式 v2（Skeleton 增量）codec 测试（Task 4 B3）
//
// 契约：docs/format/native-project-v2-skeleton.md、Agent_Task/Task4_Implementation_Plan.md B3
//
// B3 验收逐项对应：
//   1. 原有全部 v1 fixtures 仍可读取
//   2. v1→内存 v2→未保存退出时原文件逐字节不变
//   3. v1→v2 编码→重开语义一致
//   4. unsupported version/capability 各报规定错误码
//   5. 未知字段跨重排往返正确
//   6. core_tests 与 format_tests 无回归（由运行脚本整体回归保证）

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "loongbones/domain/ids.hpp"
#include "loongbones/domain/skeleton.hpp"
#include "loongbones/format/codec.hpp"
#include "loongbones/format/project_document.hpp"
#include "loongbones/format/project_io.hpp"
#include "tests/domain/test_framework.hpp"

namespace fs = std::filesystem;

namespace {

using loongbones::BlendMode;
using loongbones::Id;
using loongbones::IdKind;
using loongbones::SkinEntry;
using loongbones::Skeleton;
using loongbones::SourceRef;
using loongbones::format::decodeDocument;
using loongbones::format::DiagSource;
using loongbones::format::encodeDocument;
using loongbones::format::FormatDiagnosticCode;
using loongbones::format::kBaselineCapability;
using loongbones::format::kFormatId;
using loongbones::format::kSkeletonCapability;
using loongbones::format::OwnerAnchor;
using loongbones::format::OwnerKind;
using loongbones::format::ReaderLimits;
using loongbones::format::SourceFormatVersion;

// ---- 文档构造 ----

struct Ids {
  std::string project;
  std::string scene;
  std::string root;
};

/// 构造一个 v1 最小文档（无 capabilities、无 project.skeletons）。
/// 与 main.cpp 的 makeMinimalDoc 同构，但把 ID 暴露出来供断言使用。
std::string makeV1Doc(Ids& ids) {
  ids.project = Id::generate(IdKind::Project).str();
  ids.scene = Id::generate(IdKind::Scene).str();
  ids.root = Id::generate(IdKind::Node).str();

  std::ostringstream o;
  o << "{\n"
    << "  \"formatId\": \"" << kFormatId << "\",\n"
    << "  \"formatVersion\": 1,\n"
    << "  \"minimumReaderVersion\": 1,\n"
    << "  \"project\": {\n"
    << "    \"id\": \"" << ids.project << "\",\n"
    << "    \"name\": \"v1\",\n"
    << "    \"rootNodeId\": \"" << ids.root << "\",\n"
    << "    \"defaultSceneIndex\": 0,\n"
    << "    \"scenes\": [{\"id\": \"" << ids.scene << "\", \"name\": \"default\","
    << " \"rootNodeId\": \"" << ids.root << "\"}],\n"
    << "    \"nodes\": [{\"id\": \"" << ids.root << "\", \"name\": \"root\","
    << " \"parent\": null, \"children\": [], \"transform\": {}, \"type\": \"root\"}],\n"
    << "    \"nodeOrder\": [\"" << ids.root << "\"],\n"
    << "    \"assets\": [],\n"
    << "    \"textures\": [],\n"
    << "    \"atlases\": []\n"
    << "  }\n"
    << "}\n";
  return o.str();
}

/// 在 v1 文档基础上改版本并注入 capabilities / project.skeletons，得到 v2 文档。
/// capabilitiesJson 为空表示不注入（用于测"v2 未声明骨架能力"）。
std::string makeV2Doc(Ids& ids, const std::string& capabilitiesJson,
                      const std::string& skeletonsJson) {
  std::string text = makeV1Doc(ids);
  const std::string from = "\"formatVersion\": 1";
  const std::size_t pos = text.find(from);
  text.replace(pos, from.size(), "\"formatVersion\": 2");
  const std::string fromMin = "\"minimumReaderVersion\": 1";
  const std::size_t posMin = text.find(fromMin);
  text.replace(posMin, fromMin.size(), "\"minimumReaderVersion\": 2");

  if (!capabilitiesJson.empty()) {
    const std::string anchor = "  \"project\": {";
    const std::size_t at = text.find(anchor);
    text.insert(at, capabilitiesJson);
  }
  // project 末尾追加 skeletons（atlases 之后）。
  // skeletonsJson 为空时**不能**留尾逗号 —— `{"a": [], }` 不是合法 JSON，
  // 那样会因为语法错而掩盖掉本要测的"缺必需字段"语义。
  const std::string tail = "    \"atlases\": []\n";
  const std::size_t at = text.find(tail);
  const std::string replacement = skeletonsJson.empty() ? tail
                                                        : "    \"atlases\": [],\n" +
                                                              skeletonsJson + "\n";
  text.replace(at, tail.size(), replacement);
  return text;
}

std::string defaultCapabilities() {
  std::ostringstream o;
  o << "  \"capabilities\": {\n"
    << "    \"required\": [\"" << kBaselineCapability << "\", \"" << kSkeletonCapability << "\"],\n"
    << "    \"optional\": []\n"
    << "  },\n";
  return o.str();
}

/// 一个最小合法骨架：1 根骨 / 1 插槽 / default 皮肤 / 1 个 region 附件。
/// regionId 由调用方给出（须指向文档中真实存在的 AtlasRegion）。
std::string minimalSkeletonJson(const std::string& regionId) {
  const std::string sk = Id::generate(IdKind::Skeleton).str();
  const std::string root = Id::generate(IdKind::Bone).str();
  const std::string slot = Id::generate(IdKind::Slot).str();
  const std::string skin = Id::generate(IdKind::Skin).str();
  const std::string entry = Id::generate(IdKind::SkinEntry).str();

  std::ostringstream o;
  o << "    \"skeletons\": [{\n"
    << "      \"id\": \"" << sk << "\", \"name\": \"hero\",\n"
    << "      \"atlasIds\": [],\n"
    << "      \"boneOrder\": [\"" << root << "\"],\n"
    << "      \"bones\": [{\"id\": \"" << root << "\", \"name\": \"root\", \"parentId\": null}],\n"
    << "      \"drawOrder\": [\"" << slot << "\"],\n"
    << "      \"slots\": [{\"id\": \"" << slot << "\", \"name\": \"head\","
    << " \"boneId\": \"" << root << "\"}],\n"
    << "      \"skins\": [{\"id\": \"" << skin << "\", \"name\": \"default\", \"entries\": [\n"
    << "        {\"id\": \"" << entry << "\", \"slotId\": \"" << slot << "\","
    << " \"attachmentName\": \"head\",\n"
    << "         \"attachment\": {\"type\": \"region\", \"regionId\": \"" << regionId << "\","
    << " \"width\": 32, \"height\": 32}}\n"
    << "      ]}]\n"
    << "    }]";
  return o.str();
}

/// 带完整图集链（TextureAsset → Page → Region）的 v2 文档骨架部分，
/// 返回 regionId 用于生成骨架 JSON。
std::string atlasChainJson(std::string& regionIdOut) {
  const std::string tex = Id::generate(IdKind::TextureAsset).str();
  const std::string at = Id::generate(IdKind::Atlas).str();
  const std::string page = Id::generate(IdKind::AtlasPage).str();
  const std::string region = Id::generate(IdKind::AtlasRegion).str();
  regionIdOut = region;

  std::ostringstream o;
  o << "    \"textures\": [{\"id\": \"" << tex << "\", \"name\": \"t\","
    << " \"storage\": \"external\", \"location\": \"t.png\","
    << " \"pixelWidth\": 64, \"pixelHeight\": 64}],\n"
    << "    \"atlases\": [{\"id\": \"" << at << "\", \"name\": \"a\",\n"
    << "      \"pages\": [{\"id\": \"" << page << "\", \"name\": \"p\","
    << " \"textureAssetId\": \"" << tex << "\", \"width\": 64, \"height\": 64}],\n"
    << "      \"regions\": [{\"id\": \"" << region << "\", \"name\": \"hero\","
    << " \"pageId\": \"" << page << "\","
    << " \"packedW\": 32, \"packedH\": 32, \"originalW\": 32, \"originalH\": 32,"
    << " \"trimmedW\": 32, \"trimmedH\": 32}]\n"
    << "    }]";
  return o.str();
}

/// 把 v1 文档的 textures/atlases 段替换为带图集链的版本。
std::string withAtlasChain(const std::string& text, std::string& regionIdOut) {
  const std::string chain = atlasChainJson(regionIdOut);
  const std::string from = "    \"textures\": [],\n    \"atlases\": []";
  std::string out = text;
  const std::size_t pos = out.find(from);
  if (pos == std::string::npos) return text;
  out.replace(pos, from.size(), chain);
  return out;
}

void expectFormatFailure(const std::string& text, FormatDiagnosticCode expected,
                         const char* what) {
  const auto result = decodeDocument(text, ReaderLimits{});
  if (result.ok) {
    std::ostringstream m;
    m << what << "：本应被拒绝，但解码成功了";
    throw ::lbt::AssertionFailure{m.str()};
  }
  if (result.source != DiagSource::kFormat) {
    std::ostringstream m;
    m << what << "：期望 Format 层失败，实际来源=" << static_cast<int>(result.source);
    throw ::lbt::AssertionFailure{m.str()};
  }
  if (result.formatDiag.code != expected) {
    std::ostringstream m;
    m << what << "：期望 " << static_cast<int>(expected) << "，实际 "
      << static_cast<int>(result.formatDiag.code) << " (" << result.formatDiag.message << ")";
    throw ::lbt::AssertionFailure{m.str()};
  }
}

fs::path tempDir(const std::string& tag) {
  const fs::path base = fs::temp_directory_path() /
                        ("loongbones_v2_" + tag + "_" + Id::generate(IdKind::Project).str());
  fs::create_directories(base);
  return base;
}

std::string readAll(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

}  // namespace

// =====================================================================
// 1. v1 兼容：原有 v1 文档仍可读取
// =====================================================================

TEST(v2, v1_document_still_readable) {
  Ids ids;
  const auto loaded = decodeDocument(makeV1Doc(ids), ReaderLimits{});
  CHECK(loaded.ok);
  // 来源版本必须是 kV1 —— 这是"尚未保存，别把 v2 写回覆盖"的判断依据
  CHECK(loaded.document.sourceVersion == SourceFormatVersion::kV1);
  // 迁移后内存结构是 v2：有（空的）骨架集合 + 骨架能力
  CHECK(loaded.document.data.skeletons.empty());
  CHECK(loaded.document.hasCapabilities);
  bool hasSkeletonCap = false;
  for (const auto& c : loaded.document.requiredCapabilities) {
    if (c == kSkeletonCapability) hasSkeletonCap = true;
  }
  CHECK(hasSkeletonCap);
}

TEST(v2, v1_document_without_capabilities_migrates) {
  // v1 允许省略 capabilities；迁移必须**补上**而不得因此拒绝
  Ids ids;
  const auto loaded = decodeDocument(makeV1Doc(ids), ReaderLimits{});
  CHECK(loaded.ok);
  CHECK(loaded.document.hasCapabilities);
  CHECK_EQ(loaded.document.requiredCapabilities.size(), std::size_t{2});
}

TEST(v2, v1_document_rejects_skeletons_field) {
  // v1 若写了 project.skeletons，它会被当成未知字段收走，迁移后再写出就会
  // 与真字段撞键。必须显式拒绝，不猜测也不静默丢弃。
  Ids ids;
  std::string text = makeV1Doc(ids);
  const std::string tail = "    \"atlases\": []\n";
  const std::size_t at = text.find(tail);
  text.replace(at, tail.size(), "    \"atlases\": [],\n    \"skeletons\": []\n");
  expectFormatFailure(text, FormatDiagnosticCode::kFormatVersionUnsupported,
                      "v1 含 project.skeletons");
}

// =====================================================================
// 2. v1→内存 v2→未保存退出：原文件逐字节不变
// =====================================================================

TEST(v2, v1_open_without_save_leaves_file_byte_identical) {
  const fs::path dir = tempDir("nosave");
  Ids ids;
  const std::string text = makeV1Doc(ids);
  {
    std::ofstream out(dir / "project.json", std::ios::binary | std::ios::trunc);
    out << text;
  }
  const std::string before = readAll(dir / "project.json");

  // 打开（这会做 v1→内存 v2 迁移），然后**不保存**直接退出
  const auto opened = loongbones::format::openProject(dir.string(), ReaderLimits{});
  CHECK(opened.ok);
  CHECK(opened.document.sourceVersion == SourceFormatVersion::kV1);

  const std::string after = readAll(dir / "project.json");
  CHECK_EQ(before, after);  // 磁盘逐字节不变
  CHECK(before.find("\"formatVersion\": 1") != std::string::npos);  // 仍是 v1
}

// =====================================================================
// 3. v1→v2 编码→重开语义一致
// =====================================================================

TEST(v2, v1_save_writes_v2_and_reopens_equivalent) {
  const fs::path dir = tempDir("upgrade");
  Ids ids;
  const auto loaded = decodeDocument(makeV1Doc(ids), ReaderLimits{});
  CHECK(loaded.ok);

  const auto saved = loongbones::format::saveProject(dir.string(), loaded.document, "0.1.0");
  CHECK_EQ(saved.code, FormatDiagnosticCode::kNone);

  const std::string text = readAll(dir / "project.json");
  // 落盘必须是 v2，且声明 minimumReaderVersion=2 与骨架能力
  CHECK(text.find("\"formatVersion\": 2") != std::string::npos);
  CHECK(text.find("\"minimumReaderVersion\": 2") != std::string::npos);
  CHECK(text.find(kSkeletonCapability) != std::string::npos);
  CHECK(text.find("\"skeletons\":") != std::string::npos);

  // 重开：语义与迁移前一致
  const auto reopened = loongbones::format::openProject(dir.string(), ReaderLimits{});
  CHECK(reopened.ok);
  CHECK(reopened.document.sourceVersion == SourceFormatVersion::kV2);
  CHECK_EQ(reopened.document.data.id.str(), ids.project);
  CHECK_EQ(reopened.document.data.name, std::string("v1"));
  CHECK_EQ(reopened.document.data.nodes.size(), std::size_t{1});
  CHECK_EQ(reopened.document.data.scenes.size(), std::size_t{1});
  CHECK(reopened.document.data.skeletons.empty());
}

TEST(v2, migration_is_idempotent) {
  Ids ids;
  auto doc = decodeDocument(makeV1Doc(ids), ReaderLimits{}).document;
  CHECK(doc.sourceVersion == SourceFormatVersion::kV1);
  const std::size_t capCount = doc.requiredCapabilities.size();
  loongbones::format::migrateV1ToV2(doc);  // 幂等：再调一次不应变化
  loongbones::format::migrateV1ToV2(doc);
  CHECK_EQ(doc.requiredCapabilities.size(), capCount);
  CHECK(doc.data.skeletons.empty());
}

// =====================================================================
// 4. 版本 / 能力错误码
// =====================================================================

TEST(v2, v2_missing_skeleton_capability_rejected) {
  Ids ids;
  // v2 但不声明骨架能力
  const std::string text =
      makeV2Doc(ids, "  \"capabilities\": {\"required\": [\"" + std::string(kBaselineCapability) +
                         "\"], \"optional\": []},\n",
                "    \"skeletons\": []");
  expectFormatFailure(text, FormatDiagnosticCode::kFormatMissingField,
                      "v2 未声明骨架能力");
}

TEST(v2, v2_without_capabilities_block_at_all_rejected) {
  Ids ids;
  const std::string text = makeV2Doc(ids, "", "    \"skeletons\": []");
  expectFormatFailure(text, FormatDiagnosticCode::kFormatMissingField,
                      "v2 完全省略 capabilities");
}

TEST(v2, v1_rejects_skeleton_capability) {
  // 骨架能力对 v1 时代不存在：v1 文档声明它应被当作未知必需能力拒绝
  Ids ids;
  std::string text = makeV1Doc(ids);
  const std::string anchor = "  \"project\": {";
  const std::size_t at = text.find(anchor);
  text.insert(at, "  \"capabilities\": {\"required\": [\"" + std::string(kSkeletonCapability) +
                      "\"], \"optional\": []},\n");
  expectFormatFailure(text, FormatDiagnosticCode::kCapabilityUnsupported,
                      "v1 声明骨架能力");
}

TEST(v2, v2_unknown_required_capability_rejected) {
  Ids ids;
  const std::string text =
      makeV2Doc(ids, "  \"capabilities\": {\"required\": [\"" + std::string(kBaselineCapability) +
                         "\", \"" + kSkeletonCapability + "\", \"loongbones.mesh.v1\"],"
                         " \"optional\": []},\n",
                "    \"skeletons\": []");
  expectFormatFailure(text, FormatDiagnosticCode::kCapabilityUnsupported,
                      "v2 声明未知必需能力");
}

TEST(v2, v2_missing_project_skeletons_rejected) {
  Ids ids;
  // project.skeletons 是 v2 必需键（即使为空数组）
  const std::string text = makeV2Doc(ids, defaultCapabilities(), "");
  expectFormatFailure(text, FormatDiagnosticCode::kFormatMissingField,
                      "v2 缺 project.skeletons");
}

TEST(v2, v3_document_rejected) {
  Ids ids;
  std::string text = makeV2Doc(ids, defaultCapabilities(), "    \"skeletons\": []");
  const std::string from = "\"formatVersion\": 2";
  text.replace(text.find(from), from.size(), "\"formatVersion\": 3");
  expectFormatFailure(text, FormatDiagnosticCode::kFormatVersionUnsupported, "v3 文档");
}

// =====================================================================
// 5. 骨架严格解码
// =====================================================================

TEST(v2, skeleton_roundtrip_with_region) {
  Ids ids;
  std::string regionId;
  std::string text = withAtlasChain(makeV2Doc(ids, defaultCapabilities(), "PLACEHOLDER"),
                                    regionId);
  const std::string skel = minimalSkeletonJson(regionId);
  const std::size_t at = text.find("PLACEHOLDER");
  CHECK(at != std::string::npos);
  text.replace(at, std::string("PLACEHOLDER").size(), skel);

  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  CHECK_EQ(loaded.document.data.skeletons.size(), std::size_t{1});
  const Skeleton& s = loaded.document.data.skeletons[0];
  CHECK_EQ(s.bones.size(), std::size_t{1});
  CHECK(s.bones[0].isRoot());
  CHECK_EQ(s.slots.size(), std::size_t{1});
  CHECK_EQ(s.skins.size(), std::size_t{1});
  CHECK_EQ(s.skins[0].name, std::string("default"));
  CHECK_EQ(s.skins[0].entries.size(), std::size_t{1});
  CHECK(s.skins[0].entries[0].attachment.regionId.str() == regionId);
  // 原生创建：无 sourceRef
  CHECK(!s.sourceRef.has_value());

  // 编码 → 重开：语义一致
  const std::string encoded = encodeDocument(loaded.document, "");
  const auto again = decodeDocument(encoded, ReaderLimits{});
  CHECK(again.ok);
  CHECK_EQ(again.document.data.skeletons.size(), std::size_t{1});
  const Skeleton& t = again.document.data.skeletons[0];
  CHECK(t.id.str() == s.id.str());
  CHECK(t.skins[0].entries[0].id.str() == s.skins[0].entries[0].id.str());
  CHECK(t.skins[0].entries[0].attachment.width == s.skins[0].entries[0].attachment.width);
  // 省略的 sourceRef 不写出（不是 null）
  CHECK(encoded.find("sourceRef") == std::string::npos);
}

TEST(v2, source_ref_null_rejected) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV2Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  std::string skel = minimalSkeletonJson(regionId);
  skel.replace(skel.find("\"skins\""), std::string("\"skins\"").size(),
               "\"sourceRef\": null, \"skins\"");
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);
  expectFormatFailure(text, FormatDiagnosticCode::kJsonTypeMismatch, "sourceRef 为 null");
}

TEST(v2, source_ref_roundtrip) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV2Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  std::string skel = minimalSkeletonJson(regionId);
  const std::string ref =
      "\"sourceRef\": {\"formatId\": \"spine.binary\", \"formatVersion\": \"3.2.01\","
      " \"contentHash\": \"ab\", \"importedBy\": {\"pluginId\": \"p\", \"pluginVersion\": 7}}, ";
  skel.replace(skel.find("\"skins\""), std::string("\"skins\"").size(), ref + "\"skins\"");
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);

  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  const Skeleton& s = loaded.document.data.skeletons[0];
  CHECK(s.sourceRef.has_value());
  CHECK_EQ(s.sourceRef->formatId, std::string("spine.binary"));
  CHECK_EQ(s.sourceRef->importedByPluginVersion, 7);

  const std::string encoded = encodeDocument(loaded.document, "");
  const auto again = decodeDocument(encoded, ReaderLimits{});
  CHECK(again.ok);
  const Skeleton& t = again.document.data.skeletons[0];
  CHECK(t.sourceRef.has_value());
  CHECK_EQ(t.sourceRef->contentHash, std::string("ab"));
  CHECK(!t.sourceRef->sourceHeaderHash.has_value());  // 可选字段省略
  CHECK_EQ(t.sourceRef->importedByPluginVersion, 7);
}

TEST(v2, attachment_type_other_than_region_rejected) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV2Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  std::string skel = minimalSkeletonJson(regionId);
  const std::size_t at = skel.find("\"type\": \"region\"");
  CHECK(at != std::string::npos);
  skel.replace(at, std::string("\"type\": \"region\"").size(), "\"type\": \"mesh\"");
  const std::size_t at2 = text.find("PLACEHOLDER");
  text.replace(at2, std::string("PLACEHOLDER").size(), skel);
  expectFormatFailure(text, FormatDiagnosticCode::kJsonTypeMismatch, "attachment.type=mesh");
}

TEST(v2, color_out_of_range_rejected) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV2Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  std::string skel = minimalSkeletonJson(regionId);
  const std::size_t at = skel.find("\"slots\": [{");
  skel.insert(skel.find("\"boneId\"", at), "\"color\": {\"r\": 2, \"g\": 0, \"b\": 0, \"a\": 1}, ");
  const std::size_t at2 = text.find("PLACEHOLDER");
  text.replace(at2, std::string("PLACEHOLDER").size(), skel);
  expectFormatFailure(text, FormatDiagnosticCode::kJsonTypeMismatch, "color 分量 > 1");
}

TEST(v2, skeleton_missing_bone_order_rejected) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV2Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  std::string skel = minimalSkeletonJson(regionId);
  const std::size_t at = skel.find("\"boneOrder\":");
  const std::size_t end = skel.find('\n', at);
  skel.erase(at, end - at + 1);
  const std::size_t at2 = text.find("PLACEHOLDER");
  text.replace(at2, std::string("PLACEHOLDER").size(), skel);
  expectFormatFailure(text, FormatDiagnosticCode::kFormatMissingField, "缺 boneOrder");
}

TEST(v2, domain_rejects_bad_skeleton_reference) {
  // 结构合法但 Domain 不变量被破坏：插槽指向不存在的骨骼
  // -> 应由 Domain 层拒绝（不是 kFormatMissingField 之类的格式错）
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV2Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  std::string skel = minimalSkeletonJson(regionId);
  // `"boneId": "` 共 11 个字符，故 at+11 就是 ID 值的第一个字符。
  const std::size_t at = skel.find("\"boneId\": \"");
  CHECK(at != std::string::npos);
  const std::size_t start = at + 11;
  const std::size_t stop = skel.find('"', start);
  CHECK(stop != std::string::npos);
  skel.replace(start, stop - start, Id::generate(IdKind::Bone).str());
  const std::size_t at2 = text.find("PLACEHOLDER");
  text.replace(at2, std::string("PLACEHOLDER").size(), skel);

  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(!loaded.ok);
  CHECK(loaded.source == DiagSource::kDomain);
  CHECK_EQ(loaded.domainDiags.size(), std::size_t{1});
  CHECK(loaded.domainDiags[0].code ==
        loongbones::DiagnosticCode::kSkeletonSlotBoneNotFound);
}

// =====================================================================
// 6. 未知字段：跨重排往返（锚点以 ID 为准，不用数组位置）
// =====================================================================

TEST(v2, unknown_fields_follow_reordered_skin_entries) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV2Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);

  // 两个条目，各自带一个未知字段；第二个条目先出现
  const std::string slot = Id::generate(IdKind::Slot).str();
  const std::string skin = Id::generate(IdKind::Skin).str();
  const std::string bone = Id::generate(IdKind::Bone).str();
  const std::string e1 = Id::generate(IdKind::SkinEntry).str();
  const std::string e2 = Id::generate(IdKind::SkinEntry).str();

  std::ostringstream o;
  o << "    \"skeletons\": [{\n"
    << "      \"id\": \"" << Id::generate(IdKind::Skeleton).str() << "\", \"name\": \"hero\",\n"
    << "      \"boneOrder\": [\"" << bone << "\"],\n"
    << "      \"bones\": [{\"id\": \"" << bone << "\", \"name\": \"root\", \"parentId\": null}],\n"
    << "      \"drawOrder\": [\"" << slot << "\"],\n"
    << "      \"slots\": [{\"id\": \"" << slot << "\", \"name\": \"s\","
    << " \"boneId\": \"" << bone << "\"}],\n"
    << "      \"skins\": [{\"id\": \"" << skin << "\", \"name\": \"default\", \"entries\": [\n"
    // 顺序：e2 在前，e1 在后
    << "        {\"id\": \"" << e2 << "\", \"slotId\": \"" << slot << "\","
    << " \"attachmentName\": \"b\", \"tag\": \"second\",\n"
    << "         \"attachment\": {\"type\": \"region\", \"regionId\": \"" << regionId << "\"}},\n"
    << "        {\"id\": \"" << e1 << "\", \"slotId\": \"" << slot << "\","
    << " \"attachmentName\": \"a\", \"tag\": \"first\",\n"
    << "         \"attachment\": {\"type\": \"region\", \"regionId\": \"" << regionId << "\"}}\n"
    << "      ]}]\n"
    << "    }]";

  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), o.str());

  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);

  // 锚点必须以 entry 的**稳定 ID** 为宿主，而不是数组下标
  const auto key1 = std::make_pair(OwnerAnchor{OwnerKind::kSkinEntry, e1}, std::string(""));
  const auto key2 = std::make_pair(OwnerAnchor{OwnerKind::kSkinEntry, e2}, std::string(""));
  const auto it1 = loaded.document.unknowns.find(key1);
  const auto it2 = loaded.document.unknowns.find(key2);
  CHECK(it1 != loaded.document.unknowns.end());
  CHECK(it2 != loaded.document.unknowns.end());
  CHECK_EQ(it1->second.entries.size(), std::size_t{1});
  CHECK_EQ(it1->second.entries[0].first, std::string("tag"));
  CHECK_EQ(it1->second.entries[0].second.stringValue, std::string("first"));
  CHECK_EQ(it2->second.entries[0].second.stringValue, std::string("second"));

  // 现在把两个条目**重排**（e1 在前），未知字段必须仍然跟着各自的 ID
  auto doc = loaded.document;
  Skeleton& s = doc.data.skeletons[0];
  std::swap(s.skins[0].entries[0], s.skins[0].entries[1]);
  CHECK(s.skins[0].entries[0].id.str() == e1);

  const std::string encoded = encodeDocument(doc, "");
  const auto again = decodeDocument(encoded, ReaderLimits{});
  CHECK(again.ok);
  // 重开后数据顺序按写出顺序（e1 在前），未知字段仍按 ID 归位
  CHECK(again.document.data.skeletons[0].skins[0].entries[0].id.str() == e1);
  const auto j1 = again.document.unknowns.find(key1);
  CHECK(j1 != again.document.unknowns.end());
  CHECK_EQ(j1->second.entries[0].second.stringValue, std::string("first"));
  const auto j2 = again.document.unknowns.find(key2);
  CHECK(j2 != again.document.unknowns.end());
  CHECK_EQ(j2->second.entries[0].second.stringValue, std::string("second"));
}

TEST(v2, unknown_fields_follow_reordered_bones) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV2Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string b1 = Id::generate(IdKind::Bone).str();
  const std::string b2 = Id::generate(IdKind::Bone).str();
  const std::string slot = Id::generate(IdKind::Slot).str();

  std::ostringstream o;
  o << "    \"skeletons\": [{\n"
    << "      \"id\": \"" << Id::generate(IdKind::Skeleton).str() << "\", \"name\": \"hero\",\n"
    << "      \"boneOrder\": [\"" << b1 << "\", \"" << b2 << "\"],\n"
    << "      \"bones\": [{\"id\": \"" << b1 << "\", \"name\": \"root\", \"parentId\": null,"
    << " \"note\": \"root-bone\"},\n"
    << "                 {\"id\": \"" << b2 << "\", \"name\": \"child\","
    << " \"parentId\": \"" << b1 << "\", \"note\": \"child-bone\"}],\n"
    << "      \"drawOrder\": [\"" << slot << "\"],\n"
    << "      \"slots\": [{\"id\": \"" << slot << "\", \"name\": \"s\","
    << " \"boneId\": \"" << b1 << "\"}],\n"
    << "      \"skins\": [{\"id\": \"" << Id::generate(IdKind::Skin).str() << "\","
    << " \"name\": \"default\", \"entries\": []}]\n"
    << "    }]";
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), o.str());

  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  const auto k1 = std::make_pair(OwnerAnchor{OwnerKind::kBone, b1}, std::string(""));
  const auto k2 = std::make_pair(OwnerAnchor{OwnerKind::kBone, b2}, std::string(""));
  CHECK(loaded.document.unknowns.find(k1)->second.entries[0].second.stringValue ==
        std::string("root-bone"));
  CHECK(loaded.document.unknowns.find(k2)->second.entries[0].second.stringValue ==
        std::string("child-bone"));

  // 重命名骨骼：ID 不变，未知字段必须仍在
  auto doc = loaded.document;
  doc.data.skeletons[0].bones[0].name = "renamed-root";
  const std::string encoded = encodeDocument(doc, "");
  const auto again = decodeDocument(encoded, ReaderLimits{});
  CHECK(again.ok);
  CHECK_EQ(again.document.data.skeletons[0].bones[0].name, std::string("renamed-root"));
  const auto j1 = again.document.unknowns.find(k1);
  CHECK(j1 != again.document.unknowns.end());
  CHECK_EQ(j1->second.entries[0].second.stringValue, std::string("root-bone"));
}

// =====================================================================
// 7. 写入恒定
// =====================================================================

TEST(v2, encoder_always_writes_v2_even_for_v2_input) {
  Ids ids;
  const auto loaded = decodeDocument(makeV1Doc(ids), ReaderLimits{});
  CHECK(loaded.ok);
  const std::string encoded = encodeDocument(loaded.document, "");
  CHECK(encoded.find("\"formatVersion\": 2") != std::string::npos);
  CHECK(encoded.find("\"minimumReaderVersion\": 2") != std::string::npos);
  CHECK(encoded.find("\"capabilities\"") != std::string::npos);
  CHECK(encoded.find(kSkeletonCapability) != std::string::npos);
  // 反复编码确定性一致（RFC §4.5）
  CHECK_EQ(encoded, encodeDocument(loaded.document, ""));
}

TEST(v2, original_capabilities_preserved_not_rewritten) {
  // optional 能力必须原样保留（RFC §1.2：可安全忽略，但内容仍须保留）
  Ids ids;
  std::string text = makeV1Doc(ids);
  const std::string anchor = "  \"project\": {";
  const std::size_t at = text.find(anchor);
  text.insert(at, "  \"capabilities\": {\"required\": [\"" + std::string(kBaselineCapability) +
                      "\"], \"optional\": [\"loongbones.future.v1\"]},\n");

  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  CHECK_EQ(loaded.document.optionalCapabilities.size(), std::size_t{1});
  CHECK_EQ(loaded.document.optionalCapabilities[0], std::string("loongbones.future.v1"));

  const std::string encoded = encodeDocument(loaded.document, "");
  CHECK(encoded.find("loongbones.future.v1") != std::string::npos);
}
