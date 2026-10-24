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

/// 在 v2 文档基础上把版本号提到 v3（Task 5 B2）。
std::string makeV3Doc(Ids& ids, const std::string& capabilitiesJson,
                       const std::string& skeletonsJson) {
  std::string text = makeV2Doc(ids, capabilitiesJson, skeletonsJson);
  const std::string from = "\"formatVersion\": 2";
  const std::size_t pos = text.find(from);
  text.replace(pos, from.size(), "\"formatVersion\": 3");
  const std::string fromMin = "\"minimumReaderVersion\": 2";
  const std::size_t posMin = text.find(fromMin);
  text.replace(posMin, fromMin.size(), "\"minimumReaderVersion\": 3");
  return text;
}

/// v3 骨架 JSON：1 根骨 / 1 插槽 / default 皮肤 / 1 个**调用方给定**的附件对象。
/// attachmentJson 必须是完整的附件对象（自带 "type"）。
std::string skeletonJsonWithAttachment(const std::string& regionId,
                                       const std::string& attachmentJson) {
  const std::string sk = Id::generate(IdKind::Skeleton).str();
  const std::string root = Id::generate(IdKind::Bone).str();
  const std::string slot = Id::generate(IdKind::Slot).str();
  const std::string skin = Id::generate(IdKind::Skin).str();
  const std::string entry = Id::generate(IdKind::SkinEntry).str();

  // 附件模板里的 {root} / {slot} 会被替换成真实 ID（调用方拿不到它们）。
  std::string att = attachmentJson.empty()
                        ? "{\"type\": \"region\", \"regionId\": \"" + regionId + "\"}"
                        : attachmentJson;
  const auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
      s.replace(pos, from.size(), to);
      pos += to.size();
    }
  };
  replaceAll(att, "{root}", root);
  replaceAll(att, "{slot}", slot);

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
    << "         \"attachment\": " << att << "}\n"
    << "      ]}]\n"
    << "    }]";
  return o.str();
}

/// v3 骨架 JSON（带 IK / transform 约束）。
/// ikExtraKeys / trExtraKeys 用来注入**未知字段**（例如 ", \"futureField\": 7"），
/// 以验证「约束对象的未知字段只写不读」这一缺陷（G2）是否已修好。
std::string skeletonJsonWithConstraints(const std::string& regionId,
                                        const std::string& ikExtraKeys,
                                        const std::string& trExtraKeys) {
  const std::string sk = Id::generate(IdKind::Skeleton).str();
  const std::string root = Id::generate(IdKind::Bone).str();
  const std::string slot = Id::generate(IdKind::Slot).str();
  const std::string skin = Id::generate(IdKind::Skin).str();
  const std::string entry = Id::generate(IdKind::SkinEntry).str();
  const std::string ikId = Id::generate(IdKind::IkConstraint).str();
  const std::string trId = Id::generate(IdKind::TransformConstraint).str();

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
    << "         \"attachment\": {\"type\": \"region\", \"regionId\": \"" << regionId << "\"}}\n"
    << "      ]}],\n"
    << "      \"ikConstraints\": [{\"id\": \"" << ikId << "\", \"name\": \"leg\","
    << " \"boneIds\": [\"" << root << "\"], \"targetBoneId\": \"" << root << "\","
    << " \"mix\": 1.0, \"enabled\": true, \"bendPositive\": true" << ikExtraKeys << "}],\n"
    << "      \"transformConstraints\": [{\"id\": \"" << trId << "\", \"name\": \"copy\","
    << " \"boneId\": \"" << root << "\", \"targetBoneId\": \"" << root << "\","
    << " \"rotateMix\": -180.0, \"translateMix\": 305.1, \"scaleMix\": -316.4,"
    << " \"shearMix\": 0.0, \"offsetRotation\": 0.0, \"offsetX\": 0.0, \"offsetY\": 1.0,"
    << " \"offsetScaleX\": 0.5, \"offsetScaleY\": 1.0, \"offsetShearY\": 0.0"
    << trExtraKeys << "}]\n"
    << "    }]";
  return o.str();
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
  // 落盘必须是 v3（Task 5 B2 起统一写 v3），且声明 minimumReaderVersion=3 与骨架能力
  CHECK(text.find("\"formatVersion\": 3") != std::string::npos);
  CHECK(text.find("\"minimumReaderVersion\": 3") != std::string::npos);
  CHECK(text.find(kSkeletonCapability) != std::string::npos);
  CHECK(text.find("\"skeletons\":") != std::string::npos);

  // 重开：语义与迁移前一致
  const auto reopened = loongbones::format::openProject(dir.string(), ReaderLimits{});
  CHECK(reopened.ok);
  // Task 5 B2 起落盘与重开都是 v3
  CHECK(reopened.document.sourceVersion == SourceFormatVersion::kV3);
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
  // Task 5 B2 起本 Reader 的上限是 v3，故"v3 被拒绝"不再成立；
  // 这里改为钉住**超出上限**仍必须拒绝（v4）。
  const std::string from = "\"formatVersion\": 2";
  text.replace(text.find(from), from.size(), "\"formatVersion\": 4");
  expectFormatFailure(text, FormatDiagnosticCode::kFormatVersionUnsupported,
                      "formatVersion=4（超出本 Reader 上限 3）");
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
  CHECK(std::get<loongbones::RegionAttachment>(s.skins[0].entries[0].attachment).regionId.str() == regionId);
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
  CHECK(std::get<loongbones::RegionAttachment>(t.skins[0].entries[0].attachment).width ==
        std::get<loongbones::RegionAttachment>(s.skins[0].entries[0].attachment).width);
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
  // Task 5 B2 起 mesh 是**合法**类型，故改用真正未知的类型名钉住"未知类型必须拒绝"。
  skel.replace(at, std::string("\"type\": \"region\"").size(), "\"type\": \"ghost\"");
  const std::size_t at2 = text.find("PLACEHOLDER");
  text.replace(at2, std::string("PLACEHOLDER").size(), skel);
  expectFormatFailure(text, FormatDiagnosticCode::kJsonTypeMismatch, "attachment.type=ghost");
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
  // Task 5 B2 起统一写 v3
  CHECK(encoded.find("\"formatVersion\": 3") != std::string::npos);
  CHECK(encoded.find("\"minimumReaderVersion\": 3") != std::string::npos);
  CHECK(encoded.find("\"capabilities\"") != std::string::npos);
  CHECK(encoded.find(kSkeletonCapability) != std::string::npos);
  // 反复编码确定性一致（RFC §4.5）
  CHECK_EQ(encoded, encodeDocument(loaded.document, ""));
}

// =====================================================================
// Task 5 B2：v3 第二批附件与约束（含评审 P0 / G2 的回归）
// =====================================================================

TEST(v3, mesh_attachment_roundtrip) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string att = "{\"type\": \"mesh\", \"regionId\": \"" + regionId + "\","
                          " \"uvs\": [0.0, 0.0, 1.0, 0.0, 0.5, 1.0],"
                          " \"triangles\": [0, 1, 2],"
                          " \"vertices\": [0.0, 0.0, 32.0, 0.0, 16.0, 32.0],"
                          " \"width\": 32, \"height\": 32}";
  const std::string skel = skeletonJsonWithAttachment(regionId, att);
  const std::size_t at = text.find("PLACEHOLDER");
  CHECK(at != std::string::npos);
  text.replace(at, std::string("PLACEHOLDER").size(), skel);

  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  const auto* mesh = std::get_if<loongbones::MeshAttachment>(
      &loaded.document.data.skeletons[0].skins[0].entries[0].attachment);
  CHECK(mesh != nullptr);
  if (mesh != nullptr) {
    CHECK_EQ(mesh->triangles.size(), std::size_t{3});
    CHECK_EQ(mesh->vertices.size(), std::size_t{6});
    CHECK_EQ(mesh->uvs[5], 1.0);
    const std::string encoded = encodeDocument(loaded.document, "");
    const auto again = decodeDocument(encoded, ReaderLimits{});
    CHECK(again.ok);
    const auto* mesh2 = std::get_if<loongbones::MeshAttachment>(
        &again.document.data.skeletons[0].skins[0].entries[0].attachment);
    CHECK(mesh2 != nullptr);
    if (mesh2 != nullptr) {
      CHECK_EQ(mesh2->uvs, mesh->uvs);
      CHECK_EQ(mesh2->triangles, mesh->triangles);
      CHECK_EQ(mesh2->vertices, mesh->vertices);
    }
  }
}

TEST(v3, weighted_mesh_roundtrip_preserves_negative_weight) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string att = "{\"type\": \"weightedMesh\", \"regionId\": \"" + regionId + "\","
                          " \"uvs\": [0.0, 0.0, 1.0, 0.0, 0.5, 1.0],"
                          " \"triangles\": [0, 1, 2],"
                          " \"vertexBindings\": [[{\"boneId\": \"{root}\","
                          " \"x\": 0.0, \"y\": 0.0, \"weight\": 1.2},"
                          " {\"boneId\": \"{root}\", \"x\": 10.0, \"y\": 0.0,"
                          " \"weight\": -0.2}],"
                          " [{\"boneId\": \"{root}\", \"x\": 32.0, \"y\": 0.0,"
                          " \"weight\": 1.0}],"
                          " [{\"boneId\": \"{root}\", \"x\": 16.0, \"y\": 32.0,"
                          " \"weight\": 1.0}]]}";
  const std::string skel = skeletonJsonWithAttachment(regionId, att);
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);

  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  const auto* mesh = std::get_if<loongbones::WeightedMeshAttachment>(
      &loaded.document.data.skeletons[0].skins[0].entries[0].attachment);
  CHECK(mesh != nullptr);
  if (mesh != nullptr) {
    CHECK_EQ(mesh->vertexBindings.size(), std::size_t{3});
    CHECK_EQ(mesh->vertexBindings[0].size(), std::size_t{2});
    CHECK_EQ(mesh->vertexBindings[0][1].weight, -0.2);  // 负权重原值保留（D34）
  }
}

TEST(v3, linked_mesh_roundtrip_uses_parent_key) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string att = "{\"type\": \"linkedMesh\", \"regionId\": \"" + regionId + "\","
                          " \"parentKey\": {\"slotId\": \"{slot}\","
                          " \"attachmentName\": \"head\"},"
                          " \"inheritDeform\": true}";
  const std::string skel = skeletonJsonWithAttachment(regionId, att);
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);

  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  const auto* lm = std::get_if<loongbones::LinkedMeshAttachment>(
      &loaded.document.data.skeletons[0].skins[0].entries[0].attachment);
  CHECK(lm != nullptr);
  if (lm != nullptr) {
    CHECK_EQ(lm->parentAttachmentName, std::string{"head"});
    CHECK(lm->inheritDeform);
  }
}

TEST(v3, ik_and_transform_constraints_roundtrip) {
  Ids ids;
  std::string regionId;
  // 注意：先取 regionId，再生成骨架 JSON —— 实参求值顺序未指定，
  // 反过来写会让骨架拿到空的 regionId。
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string skel = skeletonJsonWithConstraints(regionId, "", "");
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);
  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  if (!loaded.ok) return;
  const Skeleton& s = loaded.document.data.skeletons[0];
  CHECK_EQ(s.ikConstraints.size(), std::size_t{1});
  CHECK_EQ(s.transformConstraints.size(), std::size_t{1});
  if (!s.ikConstraints.empty()) {
    CHECK(s.ikConstraints[0].enabled);      // D35：mix=1 ⇒ enabled
    CHECK_EQ(s.ikConstraints[0].mix, 1.0);
  }
  if (!s.transformConstraints.empty()) {
    CHECK_EQ(s.transformConstraints[0].rotateMix, -180.0);  // D36：原值透传
  }
  const std::string encoded = encodeDocument(loaded.document, "");
  const auto again = decodeDocument(encoded, ReaderLimits{});
  CHECK(again.ok);
  if (again.ok) {
    CHECK_EQ(again.document.data.skeletons[0].ikConstraints.size(), std::size_t{1});
    CHECK_EQ(again.document.data.skeletons[0].transformConstraints.size(), std::size_t{1});
  }
}

TEST(v3, write_always_emits_constraint_keys) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string skel = skeletonJsonWithAttachment(regionId, "");
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);
  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  const std::string encoded = encodeDocument(loaded.document, "");
  CHECK(encoded.find("\"ikConstraints\": []") != std::string::npos);
  CHECK(encoded.find("\"transformConstraints\": []") != std::string::npos);
}

TEST(v3, multi_roundtrip_has_no_duplicate_keys) {
  // 评审 P0：多轮往返不得产生重复键（重复键会在下一轮 decode 被 duplicate key 拒绝）。
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string att = "{\"type\": \"mesh\", \"regionId\": \"" + regionId + "\","
                          " \"uvs\": [0.0, 0.0, 1.0, 0.0, 0.5, 1.0],"
                          " \"triangles\": [0, 1, 2],"
                          " \"vertices\": [0.0, 0.0, 32.0, 0.0, 16.0, 32.0]}";
  const std::string skel = skeletonJsonWithAttachment(regionId, att);
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);

  const auto r0 = decodeDocument(text, ReaderLimits{});
  CHECK(r0.ok);
  const std::string e1 = encodeDocument(r0.document, "");
  const auto r1 = decodeDocument(e1, ReaderLimits{});
  CHECK(r1.ok);  // 若 e1 里有重复键，这里会失败
  const std::string e2 = encodeDocument(r1.document, "");
  const auto r2 = decodeDocument(e2, ReaderLimits{});
  CHECK(r2.ok);
  const std::string e3 = encodeDocument(r2.document, "");
  CHECK_EQ(e1, e3);  // 多轮往返稳定
}

TEST(v3, region_attachment_with_mesh_only_field_is_kept_as_unknown) {
  // 评审 P0：region 附件里出现 `uvs` 必须走**未知字段**通道（保留并回写），
  // 不能被并集键表当成已知键而静默丢弃，也不能被当成 mesh 解析。
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string att = "{\"type\": \"region\", \"regionId\": \"" + regionId + "\","
                          " \"width\": 32, \"height\": 32, \"uvs\": [0.0, 0.0]}";
  const std::string skel = skeletonJsonWithAttachment(regionId, att);
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);

  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  const std::string encoded = encodeDocument(loaded.document, "");
  CHECK(encoded.find("\"uvs\"") != std::string::npos);  // 被保留并回写
  const auto again = decodeDocument(encoded, ReaderLimits{});
  CHECK(again.ok);
  const auto* region = std::get_if<loongbones::RegionAttachment>(
      &again.document.data.skeletons[0].skins[0].entries[0].attachment);
  CHECK(region != nullptr);  // 仍然是 region（没被当成 mesh）
}

TEST(v3, unknown_field_in_constraint_is_read_and_written) {
  // 评审 G2：约束对象的未知字段此前**只写不读**。
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string skel = skeletonJsonWithConstraints(regionId, ", \"futureField\": 7", "");
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);
  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  if (!loaded.ok) return;
  const std::string encoded = encodeDocument(loaded.document, "");
  CHECK(encoded.find("\"futureField\"") != std::string::npos);
  const auto again = decodeDocument(encoded, ReaderLimits{});
  CHECK(again.ok);
  const std::string encoded2 = encodeDocument(again.document, "");
  CHECK(encoded2.find("\"futureField\"") != std::string::npos);
}

TEST(v3, v2_document_is_read_as_v3_structure) {
  Ids ids;
  const auto loaded = decodeDocument(makeV2Doc(ids, defaultCapabilities(), "    \"skeletons\": []"),
                                     ReaderLimits{});
  CHECK(loaded.ok);
  CHECK(loaded.document.sourceVersion == SourceFormatVersion::kV3);
  CHECK(loaded.document.data.skeletons.empty());
}

TEST(v3, migrate_v2_to_v3_is_idempotent) {
  Ids ids;
  const auto loaded = decodeDocument(makeV2Doc(ids, defaultCapabilities(), "    \"skeletons\": []"),
                                     ReaderLimits{});
  CHECK(loaded.ok);
  loongbones::format::ProjectDocument doc = loaded.document;
  loongbones::format::migrateV2ToV3(doc);
  const std::string first = encodeDocument(doc, "");
  loongbones::format::migrateV2ToV3(doc);  // 幂等：再来一次不产生任何变化
  const std::string second = encodeDocument(doc, "");
  CHECK_EQ(first, second);
  CHECK(doc.sourceVersion == SourceFormatVersion::kV3);
}

TEST(v3, mesh_triangle_out_of_range_rejected_by_domain) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string att = "{\"type\": \"mesh\", \"regionId\": \"" + regionId + "\","
                          " \"uvs\": [0.0, 0.0, 1.0, 0.0, 0.5, 1.0],"
                          " \"triangles\": [0, 1, 9],"
                          " \"vertices\": [0.0, 0.0, 32.0, 0.0, 16.0, 32.0]}";
  const std::string skel = skeletonJsonWithAttachment(regionId, att);
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);
  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(!loaded.ok);
  CHECK(loaded.source == DiagSource::kDomain);
}

TEST(v3, weighted_mesh_zero_weight_sum_rejected_by_domain) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string att = "{\"type\": \"weightedMesh\", \"regionId\": \"" + regionId + "\","
                          " \"uvs\": [0.0, 0.0, 1.0, 0.0, 0.5, 1.0],"
                          " \"triangles\": [0, 1, 2],"
                          " \"vertexBindings\": [[{\"boneId\": \"{root}\","
                          " \"x\": 0.0, \"y\": 0.0, \"weight\": 0.0}]]}";
  const std::string skel = skeletonJsonWithAttachment(regionId, att);
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);
  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(!loaded.ok);
  CHECK(loaded.source == DiagSource::kDomain);
}

TEST(v3, mesh_attachment_with_region_only_field_is_kept_as_unknown) {
  // 评审 P0（反向）：mesh 附件里出现 region 专属的 `color` 必须走未知字段通道，
  // 既不能被当作 mesh 字段解析，也不能被并集键表吞掉。
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string att = "{\"type\": \"mesh\", \"regionId\": \"" + regionId + "\","
                          " \"uvs\": [0.0, 0.0, 1.0, 0.0, 0.5, 1.0],"
                          " \"triangles\": [0, 1, 2],"
                          " \"vertices\": [0.0, 0.0, 32.0, 0.0, 16.0, 32.0],"
                          " \"color\": {\"r\": 1, \"g\": 0, \"b\": 0, \"a\": 1}}";
  const std::string skel = skeletonJsonWithAttachment(regionId, att);
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);
  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  const std::string encoded = encodeDocument(loaded.document, "");
  CHECK(encoded.find("\"color\"") != std::string::npos);
  const auto again = decodeDocument(encoded, ReaderLimits{});
  CHECK(again.ok);
  const auto* mesh = std::get_if<loongbones::MeshAttachment>(
      &again.document.data.skeletons[0].skins[0].entries[0].attachment);
  CHECK(mesh != nullptr);  // 仍然是 mesh（没被当成 region）
}

TEST(v3, unknown_field_in_transform_constraint_is_read_and_written) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string skel = skeletonJsonWithConstraints(regionId, "", ", \"futureMix\": 3");
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);
  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  if (!loaded.ok) return;
  const std::string encoded = encodeDocument(loaded.document, "");
  CHECK(encoded.find("\"futureMix\"") != std::string::npos);
}

TEST(v3, unknown_field_in_parent_key_is_read_and_written) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string att = "{\"type\": \"linkedMesh\", \"regionId\": \"" + regionId + "\","
                          " \"parentKey\": {\"slotId\": \"{slot}\","
                          " \"attachmentName\": \"head\", \"futureKey\": 1},"
                          " \"inheritDeform\": false}";
  const std::string skel = skeletonJsonWithAttachment(regionId, att);
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);
  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  if (!loaded.ok) return;
  const std::string encoded = encodeDocument(loaded.document, "");
  CHECK(encoded.find("\"futureKey\"") != std::string::npos);
}

TEST(v3, format_version_3_document_is_accepted) {
  Ids ids;
  std::string regionId;
  std::string text =
      withAtlasChain(makeV3Doc(ids, defaultCapabilities(), "PLACEHOLDER"), regionId);
  const std::string skel = skeletonJsonWithAttachment(regionId, "");
  const std::size_t at = text.find("PLACEHOLDER");
  text.replace(at, std::string("PLACEHOLDER").size(), skel);
  const auto loaded = decodeDocument(text, ReaderLimits{});
  CHECK(loaded.ok);
  CHECK(loaded.document.sourceVersion == SourceFormatVersion::kV3);
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
