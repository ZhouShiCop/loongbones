// 严格 JSON 解析器（RFC §4）
//
// 用 RapidJSON 的 **SAX 接口**自建值树，而不是直接用它的 DOM（RFC §7.3）：
//   · RapidJSON 的 DOM 对重复 key 的默认行为是「后者覆盖前者」，违反 R3；
//   · 其 GenericValue 的数值分型会丢失超大整数字面量的精度，违反 RFC §6.6。
// 因此本文件只用它的 Reader 做词法/结构，语义约束全部由我们自己实现。

#ifndef LOONGBONES_FORMAT_JSON_READER_HPP
#define LOONGBONES_FORMAT_JSON_READER_HPP

#include <cstddef>
#include <string>

#include "loongbones/format/format_diagnostics.hpp"
#include "loongbones/format/json_value.hpp"

namespace loongbones {
namespace format {

/// Reader 侧可配置安全上限（RFC §4.4 / U5）。
/// **不是**持久化 Schema 的一部分：不写进文档、不参与兼容判定、不随 formatVersion 演进。
///
/// `maxStringBytes` 同时约束字符串值、对象键名与**数值字面量的原始长度**（后者的
/// 检查发生在任何占位替换之前，占位后 RawNumber 只能看到 `0`，故不能延后检查）。
///
/// 本 Reader **不对**"量级越界（1e400 之类）的数值个数"设置独立上限：越界数值的
/// 处理能力完全由 maxDocBytes 自然约束（RFC §4.4）。
struct ReaderLimits {
  std::size_t maxDepth = 64;
  std::size_t maxDocBytes = 64u * 1024u * 1024u;
  std::size_t maxStringBytes = 1u * 1024u * 1024u;
};

struct JsonParseResult {
  bool ok = false;
  JsonValue root;
  FormatDiagnostic diag;
};

/// 严格解析 UTF-8 JSON 文本（拒绝 BOM）。
///
/// 强制执行的约束：
///   · RFC 8259 数值文法（RapidJSON 默认即拒绝 NaN / Infinity 等扩展）
///   · 重复 key 拒绝（RFC §4.2）
///   · 严格 UTF-8（RFC §4.3）
///   · 深度 / 文档 / 字符串上限（RFC §4.4）
/// 数值**只保留原始字面量**，不做类型化转换（RFC §6.6）。
JsonParseResult parseJson(const std::string& text, const ReaderLimits& limits);

}  // namespace format
}  // namespace loongbones

#endif  // LOONGBONES_FORMAT_JSON_READER_HPP
