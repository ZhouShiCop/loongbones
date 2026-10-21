// 格式层诊断码与诊断（RFC §9）
//
// 分层归属（RFC §9.1）：**本枚举属于 Format 层，不得进入 domain::DiagnosticCode**。
// 格式错误（解析 / 结构 / 版本 / 能力 / IO）与领域不变量是两类问题：
//   · Format 层失败 → 本文件的 FormatDiagnosticCode
//   · Domain 层失败 → domain::DiagnosticCode（既有，不改含义）
// Format 任一环节失败即中止，不进入 Domain（RFC §9.2）。

#ifndef LOONGBONES_FORMAT_FORMAT_DIAGNOSTICS_HPP
#define LOONGBONES_FORMAT_FORMAT_DIAGNOSTICS_HPP

#include <cstddef>
#include <string>

namespace loongbones {
namespace format {

/// 格式层诊断码。**只追加**，不得复用或改写既有含义（RFC §9.3、Task 3 §4.2）。
enum class FormatDiagnosticCode {
  kNone = 0,

  // ---- JSON 语法 / 类型 ----
  kJsonSyntaxError,       ///< 词法或结构不符合 RFC 8259
  kJsonTypeMismatch,      ///< 字段类型与 schema 不符
  kJsonInvalidUtf8,       ///< 非法 UTF-8 / 孤立代理项 / BOM
  kJsonDuplicateKey,      ///< 对象内重复 key（RFC §4.2）
  kJsonNonFiniteNumber,   ///< 出现 NaN / Infinity **非标准**字面量（RFC §4.1）
  kJsonNumberOutOfRange,  ///< 已知字段的字面量合法但超出其类型可表示范围（RFC §6.6）
  kJsonDepthLimit,        ///< 嵌套深度超限（RFC §4.4，Reader 可配置）
  kJsonSizeLimit,         ///< 文档 / 字符串超限（RFC §4.4，Reader 可配置）

  // ---- 版本 / 兼容 ----
  kFormatIdUnsupported,       ///< formatId 不匹配
  kFormatVersionUnsupported,  ///< formatVersion 越界
  kFormatTooNew,              ///< minimumReaderVersion 高于本 Reader
  kCapabilityUnsupported,     ///< capabilities.required 含未知必需能力

  // ---- 结构 ----
  kFormatMissingField,  ///< 缺必需字段

  // ---- IO ----
  kFormatWriteFailed,  ///< 写入 / 临时文件 / 替换失败
  kFormatReadFailed,   ///< 读取失败
};

/// 诊断码的稳定字符串形式。程序逻辑请用枚举，不得按字符串分支（规范 §10.3）。
const char* toString(FormatDiagnosticCode code) noexcept;

/// 一条格式诊断。location 为 JSON Pointer（RFC 6901），形如 /project/nodes/3/type。
struct FormatDiagnostic {
  FormatDiagnosticCode code = FormatDiagnosticCode::kNone;
  std::string message;
  std::string location;    ///< JSON Pointer；空串表示文档级
  std::size_t offset = 0;  ///< 字节偏移
  bool hasOffset = false;  ///< 仅解析期可得偏移；结构/版本期没有有意义的偏移

  FormatDiagnostic() = default;

  FormatDiagnostic(FormatDiagnosticCode c, std::string msg, std::string loc)
      : code(c), message(std::move(msg)), location(std::move(loc)) {}

  static FormatDiagnostic withOffset(FormatDiagnosticCode c, std::string msg, std::string loc,
                                     std::size_t off) {
    FormatDiagnostic d(c, std::move(msg), std::move(loc));
    d.offset = off;
    d.hasOffset = true;
    return d;
  }

  /// "CODE at <pointer>@<offset>: message"（RFC §4.6 的位置格式）
  [[nodiscard]] std::string str() const;
};

}  // namespace format
}  // namespace loongbones

#endif  // LOONGBONES_FORMAT_FORMAT_DIAGNOSTICS_HPP
