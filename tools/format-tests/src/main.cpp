// 原生项目格式 v1 codec 的自动化测试（Task 3 阶段 B）
//
// 覆盖 RFC §4 严格解析、§6.5.1 宿主锚点、§6.6 数值精度、§9 诊断分层、
// §4.5 确定性输出与 Task 3 §4.1 的目录工程原子保存。
//
// 与阶段 A 探针的关系：探针是**只读**校验器，这里测的是 codec 的读写行为。
// 两者共享 docs/format/fixtures/ 的语义，但实现各自独立。

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "loongbones/domain/ids.hpp"
#include "loongbones/format/codec.hpp"
#include "loongbones/format/json_reader.hpp"
#include "loongbones/format/project_io.hpp"
#include "tests/domain/test_framework.hpp"

namespace fs = std::filesystem;

namespace {

using loongbones::Id;
using loongbones::IdKind;
using loongbones::format::decodeDocument;
using loongbones::format::encodeDocument;
using loongbones::format::FormatDiagnosticCode;
using loongbones::format::kFormatId;
using loongbones::format::OwnerKind;
using loongbones::format::ReaderLimits;

// ---- 文档构造 ----

/// 最小合法文档。transformJson 用于替换根节点的 transform。
std::string makeMinimalDoc(const std::string& transformJson = "{}") {
  const std::string pid = Id::generate(IdKind::Project).str();
  const std::string sid = Id::generate(IdKind::Scene).str();
  const std::string root = Id::generate(IdKind::Node).str();

  std::ostringstream o;
  o << "{\n"
    << "  \"formatId\": \"" << kFormatId << "\",\n"
    << "  \"formatVersion\": 1,\n"
    << "  \"minimumReaderVersion\": 1,\n"
    << "  \"project\": {\n"
    << "    \"id\": \"" << pid << "\",\n"
    << "    \"name\": \"minimal\",\n"
    << "    \"rootNodeId\": \"" << root << "\",\n"
    << "    \"defaultSceneIndex\": 0,\n"
    << "    \"scenes\": [{\"id\": \"" << sid << "\", \"name\": \"default\",\n"
    << "                  \"rootNodeId\": \"" << root << "\"}],\n"
    << "    \"nodes\": [{\"id\": \"" << root << "\", \"name\": \"root\",\n"
    << "                 \"parent\": null, \"children\": [],\n"
    << "                 \"transform\": " << transformJson << ",\n"
    << "                 \"type\": \"root\"}],\n"
    << "    \"nodeOrder\": [\"" << root << "\"],\n"
    << "    \"assets\": [],\n"
    << "    \"textures\": [],\n"
    << "    \"atlases\": []\n"
    << "  }\n"
    << "}\n";
  return o.str();
}

/// 含 3 个链式节点的文档（root -> n1 -> n2）。
/// extraIndex 指定哪个节点（0=root, 1=n1, 2=n2）携带未知字段；< 0 表示不加。
std::string makeThreeNodeDoc(int extraIndex, const std::string& extraDecl,
                             std::vector<std::string>* nodeIdsOut) {
  const std::string pid = Id::generate(IdKind::Project).str();
  const std::string sid = Id::generate(IdKind::Scene).str();
  const std::vector<std::string> n = {Id::generate(IdKind::Node).str(),
                                      Id::generate(IdKind::Node).str(),
                                      Id::generate(IdKind::Node).str()};
  if (nodeIdsOut != nullptr) *nodeIdsOut = n;

  std::ostringstream o;
  o << "{\n"
    << "  \"formatId\": \"" << kFormatId << "\",\n"
    << "  \"formatVersion\": 1,\n"
    << "  \"minimumReaderVersion\": 1,\n"
    << "  \"project\": {\n"
    << "    \"id\": \"" << pid << "\",\n"
    << "    \"name\": \"three\",\n"
    << "    \"rootNodeId\": \"" << n[0] << "\",\n"
    << "    \"defaultSceneIndex\": 0,\n"
    << "    \"scenes\": [{\"id\": \"" << sid << "\", \"name\": \"default\",\n"
    << "                  \"rootNodeId\": \"" << n[0] << "\"}],\n"
    << "    \"nodes\": [";

  // root -> n1 -> n2
  const char* names[3] = {"root", "n1", "n2"};
  const char* types[3] = {"root", "group", "group"};
  for (int i = 0; i < 3; ++i) {
    if (i > 0) o << ", ";
    o << "{\"id\": \"" << n[static_cast<std::size_t>(i)] << "\", \"name\": \"" << names[i]
      << "\", \"parent\": ";
    if (i == 0) {
      o << "null";
    } else {
      o << "\"" << n[static_cast<std::size_t>(i - 1)] << "\"";
    }
    o << ", \"children\": ";
    if (i < 2) {
      o << "[\"" << n[static_cast<std::size_t>(i + 1)] << "\"]";
    } else {
      o << "[]";
    }
    o << ", \"transform\": {}, \"type\": \"" << types[i] << "\"";
    if (i == extraIndex) o << ", " << extraDecl;
    o << "}";
  }

  o << "],\n"
    << "    \"nodeOrder\": [\"" << n[0] << "\", \"" << n[1] << "\", \"" << n[2] << "\"],\n"
    << "    \"assets\": [],\n"
    << "    \"textures\": [],\n"
    << "    \"atlases\": []\n"
    << "  }\n"
    << "}\n";
  return o.str();
}

/// 在根对象的第一个 '{' 之后插入一条声明（用于注入未知字段或重复 key）
std::string injectTop(const std::string& base, const std::string& decl) {
  const std::size_t pos = base.find('{');
  return base.substr(0, pos + 1) + "\n  " + decl + "," + base.substr(pos + 1);
}

std::string setDefaultSceneIndex(const std::string& base, const std::string& value) {
  std::string out = base;
  const std::string from = "\"defaultSceneIndex\": 0";
  const std::size_t pos = out.find(from);
  out.replace(pos, from.size(), "\"defaultSceneIndex\": " + value);
  return out;
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

std::size_t countOccurrences(const std::string& haystack, const std::string& needle) {
  std::size_t count = 0;
  std::size_t pos = 0;
  while (!needle.empty()) {
    pos = haystack.find(needle, pos);
    if (pos == std::string::npos) break;
    ++count;
    pos += needle.size();
  }
  return count;
}

// ---- 断言辅助 ----

const char* codeName(FormatDiagnosticCode code) {
  return loongbones::format::toString(code);
}

void expectFormatFailure(const std::string& text, FormatDiagnosticCode expected,
                         const std::string& what,
                         const ReaderLimits& limits = ReaderLimits{}) {
  const auto result = decodeDocument(text, limits);
  if (result.ok) {
    LBT_FAIL("应当失败但成功了: " + what);
  }
  if (result.source != loongbones::format::DiagSource::kFormat) {
    LBT_FAIL("失败来源应为 Format 层: " + what);
  }
  if (result.formatDiag.code != expected) {
    LBT_FAIL(std::string("诊断码不符: ") + what + " 期望=" + codeName(expected) +
             " 实际=" + codeName(result.formatDiag.code));
  }
}

}  // namespace

// ===================== 严格解析（RFC §4）=====================

TEST(parse, minimal_document_accepted) {
  const auto result = decodeDocument(makeMinimalDoc(), ReaderLimits{});
  CHECK(result.ok);
  CHECK(result.source == loongbones::format::DiagSource::kNone);
  CHECK_EQ(result.document.data.nodes.size(), std::size_t{1});
}

TEST(parse, duplicate_key_rejected) {
  // RFC §4.2：重复 key 必须拒绝，不得静默取后者
  expectFormatFailure(injectTop(makeMinimalDoc(), "\"formatId\": \"dup\""),
                      FormatDiagnosticCode::kJsonDuplicateKey, "顶层重复 key");
}

TEST(parse, duplicate_key_inside_object_rejected) {
  std::string text = makeMinimalDoc();
  const std::string from = "\"name\": \"minimal\"";
  const std::size_t pos = text.find(from);
  CHECK(pos != std::string::npos);
  text.insert(pos + from.size(), ", \"name\": \"again\"");
  expectFormatFailure(text, FormatDiagnosticCode::kJsonDuplicateKey, "project 内重复 key");
}

TEST(parse, non_finite_literal_rejected) {
  expectFormatFailure(injectTop(makeMinimalDoc(), "\"bad\": NaN"),
                      FormatDiagnosticCode::kJsonNonFiniteNumber, "NaN");
  expectFormatFailure(injectTop(makeMinimalDoc(), "\"bad\": Infinity"),
                      FormatDiagnosticCode::kJsonNonFiniteNumber, "Infinity");
}

TEST(parse, bom_rejected) {
  expectFormatFailure(std::string("\xEF\xBB\xBF") + makeMinimalDoc(),
                      FormatDiagnosticCode::kJsonInvalidUtf8, "UTF-8 BOM");
}

TEST(parse, trailing_content_rejected) {
  expectFormatFailure(makeMinimalDoc() + "{}", FormatDiagnosticCode::kJsonSyntaxError,
                      "根值之后的多余内容");
}

TEST(parse, empty_document_rejected) {
  expectFormatFailure("", FormatDiagnosticCode::kJsonSyntaxError, "空文档");
}

TEST(parse, depth_limit_enforced) {
  ReaderLimits limits;
  limits.maxDepth = 4;
  std::string deep;
  for (int i = 0; i < 10; ++i) deep += "[";
  deep += "1";
  for (int i = 0; i < 10; ++i) deep += "]";
  expectFormatFailure(injectTop(makeMinimalDoc(), std::string("\"deep\": ") + deep),
                      FormatDiagnosticCode::kJsonDepthLimit, "嵌套深度超限", limits);
}

TEST(parse, document_size_limit_enforced) {
  ReaderLimits limits;
  limits.maxDocBytes = 64;
  expectFormatFailure(makeMinimalDoc(), FormatDiagnosticCode::kJsonSizeLimit,
                      "文档字节数超限", limits);
}

TEST(parse, string_size_limit_enforced) {
  ReaderLimits limits;
  limits.maxStringBytes = 8;
  expectFormatFailure(injectTop(makeMinimalDoc(), "\"big\": \"0123456789\""),
                      FormatDiagnosticCode::kJsonSizeLimit, "字符串长度超限", limits);
}

// ===================== 数值精度（RFC §6.6）=====================

TEST(number, unknown_big_integer_preserved_verbatim) {
  // RFC §6.6.5 第 1 项：20 位整数（超 int64/uint64）作为未知字段逐字节回写
  const std::string text =
      injectTop(makeMinimalDoc(), "\"vendorBig\": 12345678901234567890");
  const auto result = decodeDocument(text, ReaderLimits{});
  CHECK(result.ok);

  const std::string out = encodeDocument(result.document, "");
  CHECK(contains(out, "\"vendorBig\": 12345678901234567890"));
}

TEST(number, unknown_1e400_preserved_verbatim_and_not_an_error) {
  // RFC §6.6 复审意见 6 的核心：未知数值允许只有 raw，不做类型化转换 → 不是错误
  const std::string text = injectTop(makeMinimalDoc(), "\"vendorHuge\": 1e400");
  const auto result = decodeDocument(text, ReaderLimits{});
  CHECK(result.ok);

  const std::string out = encodeDocument(result.document, "");
  CHECK(contains(out, "\"vendorHuge\": 1e400"));
}

TEST(number, unknown_high_precision_decimal_preserved) {
  const std::string text =
      injectTop(makeMinimalDoc(), "\"vendorPrecise\": 1.000000000000000000001");
  const auto result = decodeDocument(text, ReaderLimits{});
  CHECK(result.ok);
  CHECK(contains(encodeDocument(result.document, ""),
                 "\"vendorPrecise\": 1.000000000000000000001"));
}

TEST(number, known_number_field_overflow_rejected) {
  // 已知 number 字段（transform.x）超出 double → 拒绝（RFC §6.6.3）
  expectFormatFailure(makeMinimalDoc("{\"x\": 1e400}"),
                      FormatDiagnosticCode::kJsonNumberOutOfRange, "已知 number 字段 1e400");
}

TEST(number, known_integer_field_overflow_rejected) {
  // 已知 integer 字段（defaultSceneIndex）超 int64 → 拒绝
  expectFormatFailure(setDefaultSceneIndex(makeMinimalDoc(), "12345678901234567890"),
                      FormatDiagnosticCode::kJsonNumberOutOfRange, "已知 integer 字段溢出");
}

TEST(number, known_integer_field_rejects_fractional) {
  expectFormatFailure(setDefaultSceneIndex(makeMinimalDoc(), "1.5"),
                      FormatDiagnosticCode::kJsonTypeMismatch, "integer 字段收到小数");
}

TEST(number, unknown_big_integer_in_nested_object_preserved) {
  const std::string text =
      injectTop(makeMinimalDoc(), "\"vendorObj\": {\"a\": [1e400, 99999999999999999999]}");
  const auto result = decodeDocument(text, ReaderLimits{});
  CHECK(result.ok);

  const std::string out = encodeDocument(result.document, "");
  CHECK(contains(out, "1e400"));
  CHECK(contains(out, "99999999999999999999"));
}

TEST(number, oversized_number_token_length_limit_enforced) {
  // Task 3 Back §2.1：上限检查必须在"等长占位"**之前**用原始 token 长度执行。
  // 占位后重解析时 RawNumber 只会收到 `0`，长度限制就再也看不到原始字面量。
  ReaderLimits limits;
  limits.maxStringBytes = 16;
  // 37 字节的字面量：既超过 maxStringBytes，又会触发 RapidJSON 的
  // kParseErrorNumberTooBig（指数 400 > 308）。
  const std::string huge = "0." + std::string(30, '0') + "1e400";
  CHECK(huge.size() > limits.maxStringBytes);
  const std::string text = "{\"vendorHuge\": " + huge + "}";

  const auto result = decodeDocument(text, limits);
  CHECK(!result.ok);
  CHECK(result.source == loongbones::format::DiagSource::kFormat);
  CHECK_EQ(result.formatDiag.code, FormatDiagnosticCode::kJsonSizeLimit);
  // 必须是**可定位**的诊断：既有字节偏移（指向原始 token 起点），又有 JSON Pointer
  CHECK(result.formatDiag.hasOffset);
  CHECK(result.formatDiag.offset + huge.size() <= text.size());
  CHECK_EQ(text.substr(result.formatDiag.offset, huge.size()), huge);
  CHECK(!result.formatDiag.location.empty());
}

TEST(number, many_out_of_range_number_tokens_are_accepted) {
  // Task 3 Back §2.2：合法文档不得因越界数值的**个数**超过实现内部常量而被拒绝。
  // 1025 越过旧的固定 1024 次上限；全程使用默认 ReaderLimits，不抬高任何上限。
  constexpr int kCount = 1025;
  std::string text = makeMinimalDoc();
  for (int i = 0; i < kCount; ++i) {
    text = injectTop(text, "\"vendorN" + std::to_string(i) + "\": 1e400");
  }
  CHECK(text.size() < ReaderLimits{}.maxDocBytes);

  const auto result = decodeDocument(text, ReaderLimits{});
  CHECK(result.ok);

  // 编码后这些字面量必须**逐字节原样**存在：既未被类型化、也未被规范化或改写
  const std::string out = encodeDocument(result.document, "");
  CHECK_EQ(countOccurrences(out, ": 1e400"), std::size_t{kCount});
  CHECK(!contains(out, "Infinity"));
  CHECK(!contains(out, "1.0e400"));
}

TEST(number, number_like_text_inside_strings_is_not_patched) {
  // 批量占位只作用于字符串**之外**的数值字面量：字符串里的 "1e400" 必须原样不动
  std::string text =
      injectTop(makeMinimalDoc(), "\"vendorText\": \"1e400 and 12345678901234567890\"");
  text = injectTop(text, "\"vendorHuge\": 1e400");

  const auto result = decodeDocument(text, ReaderLimits{});
  CHECK(result.ok);

  const std::string out = encodeDocument(result.document, "");
  CHECK(contains(out, "\"vendorText\": \"1e400 and 12345678901234567890\""));
  CHECK(contains(out, "\"vendorHuge\": 1e400"));
}

// ===================== 宿主锚点（RFC §6.5.1）=====================

TEST(anchor, unknown_field_follows_its_node_after_reorder) {
  // RFC §6.5.1 的反例场景：未知字段原在 nodes[?]，重排后必须仍归属**原节点**
  std::vector<std::string> nodeIds;
  const std::string text = makeThreeNodeDoc(2, "\"customZ\": 42", &nodeIds);
  const auto result = decodeDocument(text, ReaderLimits{});
  CHECK(result.ok);

  // 确认解码后锚点是 (Node, n2) 且 subPath 为空
  const auto key = std::make_pair(loongbones::format::OwnerAnchor{OwnerKind::kNode, nodeIds[2]},
                                  std::string(""));
  const auto it = result.document.unknowns.find(key);
  CHECK(it != result.document.unknowns.end());
  CHECK_EQ(it->second.entries.size(), std::size_t{1});
  CHECK_EQ(it->second.entries[0].first, std::string("customZ"));

  // 重排：把 nodes / nodeOrder 一起反转（模拟编辑会话中的顺序变化）。
  // Domain 约定 nodes[i] 与 nodeOrder[i] 对应，故两者同步反转才是合法重排。
  auto reordered = result.document;
  std::reverse(reordered.data.nodes.begin(), reordered.data.nodes.end());
  std::reverse(reordered.data.nodeOrder.begin(), reordered.data.nodeOrder.end());

  const std::string out = encodeDocument(reordered, "");
  const auto reparsed = decodeDocument(out, ReaderLimits{});
  if (!reparsed.ok) {
    std::string why = "重排后重新解析失败: ";
    if (reparsed.source == loongbones::format::DiagSource::kFormat) {
      why += toString(reparsed.formatDiag.code);
      why += " @ ";
      why += reparsed.formatDiag.location;
    } else if (!reparsed.domainDiags.empty()) {
      why += reparsed.domainDiags[0].message;
    }
    LBT_FAIL(why);
  }

  // 未知字段仍挂在**同一个节点 ID** 上，而不是新的第 3 个位置上
  const auto it2 = reparsed.document.unknowns.find(key);
  CHECK(it2 != reparsed.document.unknowns.end());

  // 且确实出现在该节点的对象里（n2 现在是 nodes[0]，故 customZ 应靠近 nodes[0]）
  const std::size_t posId = out.find(nodeIds[2]);
  const std::size_t posCustom = out.find("customZ");
  CHECK(posId != std::string::npos);
  CHECK(posCustom != std::string::npos);
  CHECK(posCustom > posId);
  // 容差：customZ 必须落在 n2 自己的对象内（紧跟其 id 之后），
  // 而不是漂到别处。单个节点对象约十余行，故上限取 1000 字节。
  CHECK(posCustom - posId < 1000);
}

TEST(anchor, transform_subpath_roundtrip) {
  std::string base = makeMinimalDoc();
  const std::string from = "\"transform\": {}";
  const std::size_t pos = base.find(from);
  CHECK(pos != std::string::npos);
  base.replace(pos, from.size(), "\"transform\": {\"flipX\": true}");

  const auto result = decodeDocument(base, ReaderLimits{});
  CHECK(result.ok);

  const std::string nodeId = result.document.data.nodes[0].common.id.str();
  const auto key = std::make_pair(loongbones::format::OwnerAnchor{OwnerKind::kNode, nodeId},
                                  std::string("/transform"));
  const auto it = result.document.unknowns.find(key);
  CHECK(it != result.document.unknowns.end());
  CHECK_EQ(it->second.entries[0].first, std::string("flipX"));

  CHECK(contains(encodeDocument(result.document, ""), "\"flipX\": true"));
}

TEST(anchor, top_level_unknown_uses_document_anchor) {
  const auto result =
      decodeDocument(injectTop(makeMinimalDoc(), "\"vendorNote\": \"v\""), ReaderLimits{});
  CHECK(result.ok);
  const auto key = std::make_pair(
      loongbones::format::OwnerAnchor{OwnerKind::kDocument, std::string()}, std::string(""));
  CHECK(result.document.unknowns.find(key) != result.document.unknowns.end());
}

TEST(anchor, unknown_field_survives_known_field_edit) {
  // Task 3 §5 验收项：打开 → 修改已知字段 → 保存后仍存在
  const auto result =
      decodeDocument(injectTop(makeMinimalDoc(), "\"vendorNote\": \"keep-me\""), ReaderLimits{});
  CHECK(result.ok);

  auto edited = result.document;
  edited.data.name = "renamed";

  const std::string out = encodeDocument(edited, "");
  CHECK(contains(out, "\"vendorNote\": \"keep-me\""));
  CHECK(contains(out, "\"name\": \"renamed\""));
}

// ===================== 版本与能力（RFC §1.1 / §1.2）=====================

TEST(version, unknown_format_id_rejected) {
  std::string text = makeMinimalDoc();
  const std::string from = "\"formatId\": \"loongbones.native-project\"";
  const std::size_t pos = text.find(from);
  CHECK(pos != std::string::npos);
  text.replace(pos, from.size(), "\"formatId\": \"something.else\"");
  expectFormatFailure(text, FormatDiagnosticCode::kFormatIdUnsupported, "formatId 不匹配");
}

// 本 Reader 现在同时接受 v1 / v2 / v3（Task 5 B2 起上限为 3），
// 故"高于本 Reader"的取值是 **4**。
TEST(version, format_version_above_reader_rejected) {
  std::string text = makeMinimalDoc();
  const std::string from = "\"formatVersion\": 1";
  const std::size_t pos = text.find(from);
  CHECK(pos != std::string::npos);
  text.replace(pos, from.size(), "\"formatVersion\": 4");
  expectFormatFailure(text, FormatDiagnosticCode::kFormatVersionUnsupported,
                      "formatVersion 高于本 Reader");
}

TEST(version, format_version_below_one_rejected) {
  std::string text = makeMinimalDoc();
  const std::string from = "\"formatVersion\": 1";
  const std::size_t pos = text.find(from);
  CHECK(pos != std::string::npos);
  text.replace(pos, from.size(), "\"formatVersion\": 0");
  expectFormatFailure(text, FormatDiagnosticCode::kFormatVersionUnsupported,
                      "formatVersion < 1");
}

TEST(version, minimum_reader_above_reader_rejected) {
  std::string text = makeMinimalDoc();
  const std::string from = "\"minimumReaderVersion\": 1";
  const std::size_t pos = text.find(from);
  CHECK(pos != std::string::npos);
  text.replace(pos, from.size(), "\"minimumReaderVersion\": 9");
  expectFormatFailure(text, FormatDiagnosticCode::kFormatTooNew, "minimumReaderVersion 过高");
}

TEST(version, app_version_optional_on_read) {
  // U4：读取时可省略，缺失不得报错
  CHECK(decodeDocument(makeMinimalDoc(), ReaderLimits{}).ok);
  CHECK(decodeDocument(injectTop(makeMinimalDoc(), "\"appVersion\": \"1.2.3\""),
                       ReaderLimits{})
            .ok);
}

TEST(capabilities, unknown_required_capability_rejected) {
  const std::string decl =
      "\"capabilities\": {\"required\": [\"loongbones.unknown.v9\"], \"optional\": []}";
  expectFormatFailure(injectTop(makeMinimalDoc(), decl),
                      FormatDiagnosticCode::kCapabilityUnsupported, "未知必需能力");
}

TEST(capabilities, known_required_capability_accepted) {
  const std::string decl =
      "\"capabilities\": {\"required\": [\"loongbones.atlas.v1\"], \"optional\": []}";
  CHECK(decodeDocument(injectTop(makeMinimalDoc(), decl), ReaderLimits{}).ok);
}

TEST(capabilities, unknown_optional_capability_preserved) {
  // RFC §1.2：optional 里不认识的条目可忽略，但**内容仍须保留**
  const std::string decl =
      "\"capabilities\": {\"required\": [], \"optional\": [\"loongbones.future.v2\"]}";
  const auto result = decodeDocument(injectTop(makeMinimalDoc(), decl), ReaderLimits{});
  CHECK(result.ok);
  CHECK_EQ(result.document.optionalCapabilities.size(), std::size_t{1});
  CHECK_EQ(result.document.optionalCapabilities[0], std::string("loongbones.future.v2"));
  CHECK(contains(encodeDocument(result.document, ""), "loongbones.future.v2"));
}

// ===================== 确定性输出（RFC §4.5）=====================

TEST(encode, repeated_encoding_is_byte_identical) {
  const auto result = decodeDocument(makeMinimalDoc(), ReaderLimits{});
  CHECK(result.ok);
  const std::string a = encodeDocument(result.document, "1.0.0");
  const std::string b = encodeDocument(result.document, "1.0.0");
  CHECK_EQ(a, b);
}

TEST(encode, output_round_trips) {
  const auto first = decodeDocument(makeMinimalDoc(), ReaderLimits{});
  CHECK(first.ok);
  const std::string text = encodeDocument(first.document, "1.0.0");
  const auto second = decodeDocument(text, ReaderLimits{});
  CHECK(second.ok);
  CHECK_EQ(encodeDocument(second.document, "1.0.0"), text);
}

TEST(encode, app_version_omitted_when_empty) {
  const auto result = decodeDocument(makeMinimalDoc(), ReaderLimits{});
  CHECK(result.ok);
  const std::string out = encodeDocument(result.document, "");
  CHECK(!contains(out, "appVersion"));
}

TEST(encode, unknown_keys_emitted_after_known_keys_in_original_order) {
  std::string text = injectTop(makeMinimalDoc(), "\"zFirst\": 1");
  text = injectTop(text, "\"aSecond\": 2");
  const auto result = decodeDocument(text, ReaderLimits{});
  CHECK(result.ok);

  const std::string out = encodeDocument(result.document, "");
  const std::size_t posA = out.find("aSecond");
  const std::size_t posZ = out.find("zFirst");
  CHECK(posA != std::string::npos);
  CHECK(posZ != std::string::npos);
  // injectTop 每次都插在最前，故后注入的 aSecond 在文档中更靠前，输出须保持该顺序
  CHECK(posA < posZ);
}

// ===================== 诊断分层（RFC §9）=====================

TEST(layering, format_failure_does_not_produce_domain_diagnostics) {
  const auto result = decodeDocument(injectTop(makeMinimalDoc(), "\"dup\": NaN"), ReaderLimits{});
  CHECK(!result.ok);
  CHECK(result.source == loongbones::format::DiagSource::kFormat);
  CHECK(result.domainDiags.empty());
}

TEST(layering, malformed_id_is_reported_by_domain) {
  // ID 格式合法性属 Domain（kIdMalformed），Format 只保证存在性与类型
  std::string text = makeMinimalDoc();
  const std::string from = "\"id\": \"";
  std::size_t pos = text.find(from);
  CHECK(pos != std::string::npos);
  // 把 project.id 换成非 ULID
  const std::size_t start = pos + from.size();
  const std::size_t end = text.find('"', start);
  text.replace(start, end - start, "NOT-A-ULID");

  const auto result = decodeDocument(text, ReaderLimits{});
  CHECK(!result.ok);
  CHECK(result.source == loongbones::format::DiagSource::kDomain);
  CHECK_EQ(result.domainDiags.size(), std::size_t{1});
  CHECK_EQ(result.domainDiags[0].code, loongbones::DiagnosticCode::kIdMalformed);
}

TEST(layering, missing_required_field_is_format_layer) {
  std::string text = makeMinimalDoc();
  const std::string from = "\"name\": \"minimal\",";
  const std::size_t pos = text.find(from);
  CHECK(pos != std::string::npos);
  text.erase(pos, from.size());
  expectFormatFailure(text, FormatDiagnosticCode::kFormatMissingField, "缺 project.name");
}

TEST(layering, unknown_enum_value_is_type_mismatch) {
  std::string text = makeMinimalDoc();
  const std::string from = "\"type\": \"root\"";
  const std::size_t pos = text.find(from);
  CHECK(pos != std::string::npos);
  text.replace(pos, from.size(), "\"type\": \"banana\"");
  expectFormatFailure(text, FormatDiagnosticCode::kJsonTypeMismatch, "非法枚举值");
}

// ===================== 目录工程 IO（Task 3 §4.1）=====================

namespace {

fs::path tempDir(const std::string& tag) {
  const fs::path base = fs::temp_directory_path() / ("loongbones_fmt_" + tag + "_" +
                                                     Id::generate(IdKind::Project).str());
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

TEST(io, create_save_open_round_trip) {
  const fs::path dir = tempDir("roundtrip");
  auto doc = loongbones::format::createProjectDocument("demo");

  const auto saved = loongbones::format::saveProject(dir.string(), doc, "0.1.0");
  CHECK_EQ(saved.code, FormatDiagnosticCode::kNone);

  const auto opened = loongbones::format::openProject(dir.string(), ReaderLimits{});
  CHECK(opened.ok);
  CHECK_EQ(opened.document.data.name, std::string("demo"));
  CHECK_EQ(opened.document.data.nodes.size(), std::size_t{1});
}

TEST(io, repeated_save_is_byte_identical) {
  const fs::path dir = tempDir("stable");
  const auto doc = loongbones::format::createProjectDocument("demo");

  CHECK_EQ(loongbones::format::saveProject(dir.string(), doc, "0.1.0").code,
           FormatDiagnosticCode::kNone);
  const std::string first = readAll(dir / "project.json");
  CHECK_EQ(loongbones::format::saveProject(dir.string(), doc, "0.1.0").code,
           FormatDiagnosticCode::kNone);
  const std::string second = readAll(dir / "project.json");
  CHECK_EQ(first, second);
}

TEST(io, save_failure_leaves_source_untouched) {
  const fs::path dir = tempDir("fail");
  const auto doc = loongbones::format::createProjectDocument("demo");
  CHECK_EQ(loongbones::format::saveProject(dir.string(), doc, "0.1.0").code,
           FormatDiagnosticCode::kNone);
  const std::string before = readAll(dir / "project.json");

  // 目标目录不存在 → 保存失败
  const fs::path missing = dir / "no" / "such" / "dir";
  const auto failed = loongbones::format::saveProject(missing.string(), doc, "0.1.0");
  CHECK_EQ(failed.code, FormatDiagnosticCode::kFormatWriteFailed);

  // 磁盘上原有文件必须**逐字节不变**
  CHECK_EQ(readAll(dir / "project.json"), before);
}

TEST(io, no_temp_file_left_behind) {
  const fs::path dir = tempDir("clean");
  const auto doc = loongbones::format::createProjectDocument("demo");
  CHECK_EQ(loongbones::format::saveProject(dir.string(), doc, "0.1.0").code,
           FormatDiagnosticCode::kNone);

  int leftovers = 0;
  for (const auto& entry : fs::directory_iterator(dir)) {
    if (entry.path().filename().string().find("project.json.tmp-") == 0) ++leftovers;
  }
  CHECK_EQ(leftovers, 0);
}

TEST(io, save_as_creates_directory_and_leaves_original_intact) {
  const fs::path original = tempDir("orig");
  const fs::path copy = fs::temp_directory_path() /
                        ("loongbones_fmt_copy_" + Id::generate(IdKind::Project).str());
  CHECK(!fs::exists(copy));

  auto doc = loongbones::format::createProjectDocument("demo");
  CHECK_EQ(loongbones::format::saveProject(original.string(), doc, "0.1.0").code,
           FormatDiagnosticCode::kNone);
  const std::string before = readAll(original / "project.json");

  doc.data.name = "renamed";
  const auto saved = loongbones::format::saveProjectAs(copy.string(), doc, "0.1.0");
  CHECK_EQ(saved.code, FormatDiagnosticCode::kNone);
  CHECK(fs::exists(copy / "project.json"));

  // 另存**不得**改动原工程
  CHECK_EQ(readAll(original / "project.json"), before);

  const auto opened = loongbones::format::openProject(copy.string(), ReaderLimits{});
  CHECK(opened.ok);
  CHECK_EQ(opened.document.data.name, std::string("renamed"));
}

TEST(io, open_missing_file_reports_read_failure) {
  const fs::path dir = tempDir("missing");
  const auto opened = loongbones::format::openProject(dir.string(), ReaderLimits{});
  CHECK(!opened.ok);
  CHECK(opened.source == loongbones::format::DiagSource::kFormat);
  CHECK_EQ(opened.formatDiag.code, FormatDiagnosticCode::kFormatReadFailed);
}

TEST(io, unknown_fields_survive_save_open_cycle) {
  // Task 3 §5 端到端验收：未知字段经 保存 → 打开 仍在
  const fs::path dir = tempDir("unknowns");
  std::vector<std::string> nodeIds;
  const std::string text = makeThreeNodeDoc(2, "\"customZ\": {\"n\": 12345678901234567890}",
                                            &nodeIds);
  const auto result = decodeDocument(text, ReaderLimits{});
  CHECK(result.ok);

  CHECK_EQ(loongbones::format::saveProject(dir.string(), result.document, "0.1.0").code,
           FormatDiagnosticCode::kNone);
  const auto opened = loongbones::format::openProject(dir.string(), ReaderLimits{});
  CHECK(opened.ok);

  const auto key = std::make_pair(loongbones::format::OwnerAnchor{OwnerKind::kNode, nodeIds[2]},
                                  std::string(""));
  const auto it = opened.document.unknowns.find(key);
  CHECK(it != opened.document.unknowns.end());
  CHECK_EQ(it->second.entries[0].first, std::string("customZ"));
  CHECK(contains(readAll(dir / "project.json"), "12345678901234567890"));
}

int main() {
  const int failed = lbt::TestRegistry::instance().runAll();
  std::printf("\nformat-tests: %d passed, %d failed\n",
              lbt::TestRegistry::instance().passed(), failed);
  return failed == 0 ? 0 : 1;
}
