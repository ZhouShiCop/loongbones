// 探针专用：最小严格 JSON 解析（RFC 8259 子集）。
//
// 这是「格式探针」的一部分，用于验证 native-project-v1 契约的可判定性，
// **不是** 工程 codec（阶段 B 产物）。因此：
//   · 只读：解析为不可变的 JsonValue 树，不做领域解码、不做写出。
//   · 零第三方依赖、零 Qt、零文件系统耦合（字节由调用方传入）。
//   · 严格性是可执行规范：重复 key、非标准数值、非法 UTF-8、深度/大小上限
//     均在解析期拒绝，并给出 JSON Pointer 与字节偏移。

#ifndef LOONGBONES_FORMAT_PROBE_JSON_HPP
#define LOONGBONES_FORMAT_PROBE_JSON_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lbfmt {

enum class JsonType { kNull, kBool, kNumber, kString, kArray, kObject };

class JsonValue;

using JsonArray = std::vector<JsonValue>;
/// 对象用「有序键值对」表示：既保留文档顺序，又便于成员查找。
using JsonObject = std::vector<std::pair<std::string, JsonValue>>;

class JsonValue {
 public:
  JsonValue() = default;

  static JsonValue makeBool(bool v) noexcept;
  static JsonValue makeNumber(double v) noexcept;
  static JsonValue makeString(std::string v);
  static JsonValue makeArray(JsonArray v);
  static JsonValue makeObject(JsonObject v);

  [[nodiscard]] JsonType type() const noexcept { return type_; }
  [[nodiscard]] bool isNull() const noexcept { return type_ == JsonType::kNull; }
  [[nodiscard]] bool isBool() const noexcept { return type_ == JsonType::kBool; }
  [[nodiscard]] bool isNumber() const noexcept { return type_ == JsonType::kNumber; }
  [[nodiscard]] bool isString() const noexcept { return type_ == JsonType::kString; }
  [[nodiscard]] bool isArray() const noexcept { return type_ == JsonType::kArray; }
  [[nodiscard]] bool isObject() const noexcept { return type_ == JsonType::kObject; }

  [[nodiscard]] bool boolValue() const noexcept { return bool_; }
  [[nodiscard]] double numberValue() const noexcept { return number_; }
  [[nodiscard]] const std::string& stringValue() const noexcept { return string_; }
  [[nodiscard]] const JsonArray& arrayValue() const noexcept { return array_; }
  [[nodiscard]] const JsonObject& objectValue() const noexcept { return object_; }

  /// 对象成员查找；不存在返回 nullptr（重复 key 已在解析期拒绝，故无歧义）。
  [[nodiscard]] const JsonValue* find(std::string_view key) const noexcept;

  [[nodiscard]] std::size_t offset() const noexcept { return offset_; }
  void setOffset(std::size_t o) noexcept { offset_ = o; }

 private:
  JsonType type_ = JsonType::kNull;
  bool bool_ = false;
  double number_ = 0.0;
  std::string string_;
  JsonArray array_;
  JsonObject object_;
  std::size_t offset_ = 0;
};

/// 解析上限。属于 Reader 策略，**不是格式契约**，故不写入文档。
struct Limits {
  std::size_t maxDepth = 64;
  std::size_t maxDocumentBytes = 64u * 1024u * 1024u;
  std::size_t maxStringBytes = 1024u * 1024u;
};

struct ParseError {
  std::string code;     ///< 拟新增诊断码（字符串形式）
  std::string message;
  std::string pointer;  ///< JSON Pointer；解析失败时可能是部分路径
  std::size_t offset = 0;
};

/// 严格解析。成功返回值；失败返回 std::nullopt 并填充 err。
[[nodiscard]] std::optional<JsonValue> parseStrict(std::string_view text, const Limits& limits,
                                                   ParseError* err);

/// JSON Pointer 片段拼接（不做 ~ / 转义之外的处理）。
[[nodiscard]] std::string pointerJoin(std::string_view base, std::string_view token);

}  // namespace lbfmt

#endif  // LOONGBONES_FORMAT_PROBE_JSON_HPP
