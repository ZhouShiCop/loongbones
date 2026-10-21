// JSON 值树（RFC §6.6）
//
// 数值**始终保留逐字节原始字面量** numberRaw，按需才解析为类型化值：
//   · 未知字段：只用 numberRaw 回写，**不做任何类型化转换**（故不存在越界）；
//   · 已知字段：由调用方按字段的 schema 类型解析，越界则拒绝（kJsonNumberOutOfRange）。
// 这是 RFC §6.6.1 的核心规则 —— 没有类型转换，就没有溢出。

#ifndef LOONGBONES_FORMAT_JSON_VALUE_HPP
#define LOONGBONES_FORMAT_JSON_VALUE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace loongbones {
namespace format {

enum class JsonType { kNull, kBool, kNumber, kString, kArray, kObject };

/// 一个 JSON 值。
struct JsonValue {
  JsonType type = JsonType::kNull;
  bool boolValue = false;
  /// type == kNumber 时的**逐字节**原始字面量（必然符合 RFC 8259 数值文法）
  std::string numberRaw;
  /// type == kString 时已解码的 UTF-8 文本（\u 转义已展开）
  std::string stringValue;
  std::vector<JsonValue> array;
  /// type == kObject 时的成员，**保序**（未知字段按首次出现顺序回写，RFC §6.2）
  std::vector<std::pair<std::string, JsonValue>> members;

  [[nodiscard]] bool isNull() const noexcept { return type == JsonType::kNull; }
  [[nodiscard]] bool isBool() const noexcept { return type == JsonType::kBool; }
  [[nodiscard]] bool isNumber() const noexcept { return type == JsonType::kNumber; }
  [[nodiscard]] bool isString() const noexcept { return type == JsonType::kString; }
  [[nodiscard]] bool isArray() const noexcept { return type == JsonType::kArray; }
  [[nodiscard]] bool isObject() const noexcept { return type == JsonType::kObject; }

  /// 取成员；不存在返回 nullptr。对象成员数很少，线性查找即可。
  [[nodiscard]] const JsonValue* find(std::string_view key) const noexcept;
};

enum class DoubleParseStatus {
  kOk,
  kOutOfRange,  ///< 字面量合法但超出 double 可表示范围（如 1e400）
  kMalformed,   ///< 字面量本身非法（解析期已排除，不应发生）
};

/// 把原始字面量解析为 double。
/// 用 std::from_chars（RFC §6.6.4）：溢出时返回 result_out_of_range，
/// **不**把结果写成 inf；禁止先转 double 再用 isinf 事后判断。
DoubleParseStatus parseDoubleStatus(const std::string& raw, double& out) noexcept;

enum class NumberLiteralKind {
  kOk,          ///< 可精确表示为目标整数类型
  kOutOfRange,  ///< 字面量合法但超出目标类型范围
  kNotInteger,  ///< 是有限数但带小数部分（类型不符）
  kMalformed,   ///< 字面量本身非法
};

/// 按「integer 字段」解释原始字面量，并区分「越界」与「带小数部分」两种失败。
/// 先判越界再判小数：12345678901234567890 是**越界**而不是「不是整数」。
NumberLiteralKind classifyInt64(const std::string& raw, std::int64_t& out) noexcept;
NumberLiteralKind classifyInt32(const std::string& raw, std::int32_t& out) noexcept;

/// 双精度的最短往返表示（RFC §4.5）。保证 parse(format(v)) == v。
std::string formatShortestDouble(double value);

/// JSON Pointer 段转义（RFC 6901）：~ -> ~0，/ -> ~1
std::string escapePointerSegment(std::string_view seg);

}  // namespace format
}  // namespace loongbones

#endif  // LOONGBONES_FORMAT_JSON_VALUE_HPP
