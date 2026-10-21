#include "loongbones/format/json_value.hpp"

#include <charconv>
#include <cmath>
#include <limits>

namespace loongbones {
namespace format {
namespace {

// int64 的精确边界。用「下界闭、上界开」表达：
// 小于 2^63 的最大 double 是 2^63-1024，落在 int64 内，故上界取开。
constexpr double kInt64Min = -9223372036854775808.0;
constexpr double kInt64MaxExclusive = 9223372036854775808.0;

}  // namespace

const JsonValue* JsonValue::find(std::string_view key) const noexcept {
  if (type != JsonType::kObject) return nullptr;
  for (const auto& member : members) {
    if (std::string_view(member.first) == key) return &member.second;
  }
  return nullptr;
}

DoubleParseStatus parseDoubleStatus(const std::string& raw, double& out) noexcept {
  if (raw.empty()) return DoubleParseStatus::kMalformed;
  const char* first = raw.data();
  const char* last = first + raw.size();
  double value = 0.0;
  const auto r = std::from_chars(first, last, value, std::chars_format::general);
  if (r.ec == std::errc::result_out_of_range) return DoubleParseStatus::kOutOfRange;
  if (r.ec != std::errc{} || r.ptr != last) return DoubleParseStatus::kMalformed;
  out = value;
  return DoubleParseStatus::kOk;
}

NumberLiteralKind classifyInt64(const std::string& raw, std::int64_t& out) noexcept {
  double value = 0.0;
  const auto status = parseDoubleStatus(raw, value);
  if (status == DoubleParseStatus::kOutOfRange) return NumberLiteralKind::kOutOfRange;
  if (status == DoubleParseStatus::kMalformed) return NumberLiteralKind::kMalformed;
  // 先判越界，再判小数：12345678901234567890 是「越界」而不是「不是整数」
  if (!(value >= kInt64Min && value < kInt64MaxExclusive)) {
    return NumberLiteralKind::kOutOfRange;
  }
  double integral = 0.0;
  if (std::modf(value, &integral) != 0.0) return NumberLiteralKind::kNotInteger;
  out = static_cast<std::int64_t>(integral);
  return NumberLiteralKind::kOk;
}

NumberLiteralKind classifyInt32(const std::string& raw, std::int32_t& out) noexcept {
  std::int64_t wide = 0;
  const auto kind = classifyInt64(raw, wide);
  if (kind != NumberLiteralKind::kOk) return kind;
  if (wide < std::numeric_limits<std::int32_t>::min() ||
      wide > std::numeric_limits<std::int32_t>::max()) {
    return NumberLiteralKind::kOutOfRange;
  }
  out = static_cast<std::int32_t>(wide);
  return NumberLiteralKind::kOk;
}

std::string formatShortestDouble(double value) {
  char buffer[64];
  const auto r = std::to_chars(buffer, buffer + sizeof(buffer), value,
                               std::chars_format::general);
  if (r.ec != std::errc{}) return std::string("0");
  return std::string(buffer, r.ptr);
}

std::string escapePointerSegment(std::string_view seg) {
  std::string out;
  out.reserve(seg.size());
  for (const char c : seg) {
    if (c == '~') {
      out += "~0";
    } else if (c == '/') {
      out += "~1";
    } else {
      out += c;
    }
  }
  return out;
}

}  // namespace format
}  // namespace loongbones
