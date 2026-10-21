// 探针专用：最小严格 JSON 解析实现（RFC 8259 子集）。

#include "json.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace lbfmt {
namespace {

constexpr char kCrockfordBase32[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
constexpr std::size_t kUlidLength = 26;

int hexVal(char c) noexcept {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

void appendUtf8(std::string& out, std::uint32_t cp) {
  if (cp <= 0x7FU) {
    out.push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FFU) {
    out.push_back(static_cast<char>(0xC0U | (cp >> 6)));
    out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
  } else if (cp <= 0xFFFFU) {
    out.push_back(static_cast<char>(0xE0U | (cp >> 12)));
    out.push_back(static_cast<char>(0x80U | ((cp >> 6) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
  } else {
    out.push_back(static_cast<char>(0xF0U | (cp >> 18)));
    out.push_back(static_cast<char>(0x80U | ((cp >> 12) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | ((cp >> 6) & 0x3FU)));
    out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
  }
}

/// 严格 UTF-8 校验：拒绝截断序列、过长编码、代理项（U+D800..U+DFFF）。
bool validateUtf8(std::string_view s, std::size_t* badPos) {
  const auto u = [](char c) { return static_cast<unsigned char>(c); };
  std::size_t i = 0;
  while (i < s.size()) {
    const unsigned char c = u(s[i]);
    std::size_t need = 0;
    unsigned char lo = 0x80;
    unsigned char hi = 0xBF;  // 第二个字节的合法区间
    if (c <= 0x7F) {
      ++i;
      continue;
    } else if (c >= 0xC2 && c <= 0xDF) {
      need = 1;
    } else if (c >= 0xE0 && c <= 0xEF) {
      need = 2;
      if (c == 0xE0) lo = 0xA0;  // 排除过长编码
      if (c == 0xED) hi = 0x9F;  // 排除代理项
    } else if (c >= 0xF0 && c <= 0xF4) {
      need = 3;
      if (c == 0xF0) lo = 0x90;
      if (c == 0xF4) hi = 0x8F;
    } else {
      if (badPos != nullptr) *badPos = i;
      return false;
    }
    if (i + need >= s.size()) {
      if (badPos != nullptr) *badPos = i;
      return false;
    }
    for (std::size_t k = 1; k <= need; ++k) {
      const unsigned char cc = u(s[i + k]);
      const unsigned char l = (k == 1) ? lo : 0x80;
      const unsigned char h = (k == 1) ? hi : 0xBF;
      if (cc < l || cc > h) {
        if (badPos != nullptr) *badPos = i + k;
        return false;
      }
    }
    i += need + 1;
  }
  return true;
}

class Parser {
 public:
  Parser(std::string_view text, const Limits& limits) : s_(text), lim_(limits) {}

  bool run(JsonValue& out, ParseError& err) {
    if (s_.size() >= 3 && static_cast<unsigned char>(s_[0]) == 0xEF &&
        static_cast<unsigned char>(s_[1]) == 0xBB && static_cast<unsigned char>(s_[2]) == 0xBF) {
      return fail(err, "JSON_INVALID_UTF8", "文档以 UTF-8 BOM 开头", 0, "");
    }
    std::size_t bad = 0;
    if (!validateUtf8(s_, &bad)) {
      return fail(err, "JSON_INVALID_UTF8", "文档包含非法 UTF-8 序列", bad, "");
    }
    if (s_.size() > lim_.maxDocumentBytes) {
      return fail(err, "JSON_SIZE_LIMIT", "文档超过大小上限", 0, "");
    }
    skipWs();
    if (!parseValue(out, 1, "", err)) return false;
    skipWs();
    if (pos_ != s_.size()) {
      char buf[160];
      std::snprintf(buf, sizeof buf, "根值之后存在多余内容（首字符 0x%02X）",
                    static_cast<unsigned>(static_cast<unsigned char>(s_[pos_])));
      return fail(err, "JSON_SYNTAX_ERROR", buf, pos_, "");
    }
    return true;
  }

 private:
  std::string_view s_;
  Limits lim_{};
  std::size_t pos_ = 0;

  bool eof() const noexcept { return pos_ >= s_.size(); }
  char peek() const noexcept { return eof() ? '\0' : s_[pos_]; }

  static bool isWs(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
  }
  void skipWs() noexcept {
    while (!eof() && isWs(s_[pos_])) ++pos_;
  }

  bool matches(const char* kw) const noexcept {
    const std::size_t n = std::strlen(kw);
    return s_.size() - pos_ >= n && std::memcmp(s_.data() + pos_, kw, n) == 0;
  }

  static bool fail(ParseError& err, const char* code, const std::string& message, std::size_t off,
                   const std::string& pointer) {
    err.code = code;
    err.message = message;
    err.offset = off;
    err.pointer = pointer;
    return false;
  }

  bool parseValue(JsonValue& out, std::size_t depth, const std::string& pointer, ParseError& err) {
    if (depth > lim_.maxDepth) {
      return fail(err, "JSON_DEPTH_LIMIT", "嵌套深度超过上限", pos_, pointer);
    }
    if (eof()) {
      return fail(err, "JSON_SYNTAX_ERROR", "意外结束：期望一个值", pos_, pointer);
    }
    const std::size_t start = pos_;
    const char c = peek();
    if (c == '{') return parseObject(out, depth, pointer, err);
    if (c == '[') return parseArray(out, depth, pointer, err);
    if (c == '"') {
      std::string text;
      if (!parseString(text, err)) return false;
      out = JsonValue::makeString(std::move(text));
      out.setOffset(start);
      return true;
    }
    if (c == 't') return parseKeyword("true", true, out, start, pointer, err);
    if (c == 'f') return parseKeyword("false", false, out, start, pointer, err);
    if (c == 'n') return parseKeyword("null", false, out, start, pointer, err);
    // 非标准数值字面量：给出比「语法错误」更明确的诊断
    if (matches("NaN") || matches("Infinity") || matches("-Infinity")) {
      return fail(err, "JSON_NON_FINITE_NUMBER",
                  "非标准数值字面量（RFC 8259 无 NaN/Infinity）", start, pointer);
    }
    double v = 0.0;
    if (!parseNumber(v, err)) return false;
    out = JsonValue::makeNumber(v);
    out.setOffset(start);
    return true;
  }

  bool parseKeyword(const char* kw, bool value, JsonValue& out, std::size_t start,
                    const std::string& pointer, ParseError& err) {
    const std::size_t n = std::strlen(kw);
    if (s_.size() - pos_ < n || std::memcmp(s_.data() + pos_, kw, n) != 0) {
      std::string msg = "非法字面量：期望 ";
      msg += kw;
      return fail(err, "JSON_SYNTAX_ERROR", msg, pos_, pointer);
    }
    pos_ += n;
    if (std::strcmp(kw, "null") == 0) {
      out = JsonValue();
    } else {
      out = JsonValue::makeBool(value);
    }
    out.setOffset(start);
    return true;
  }

  /// 严格 RFC 8259 数值文法：
  ///   -? (0 | [1-9][0-9]*) ( \. [0-9]+ )? ( [eE] [+-]? [0-9]+ )?
  bool parseNumber(double& out, ParseError& err) {
    const std::size_t start = pos_;
    if (!eof() && s_[pos_] == '-') ++pos_;
    if (eof() || s_[pos_] < '0' || s_[pos_] > '9') {
      return fail(err, "JSON_SYNTAX_ERROR", "数值缺少整数部分", pos_, "");
    }
    if (s_[pos_] == '0') {
      ++pos_;
      if (!eof() && s_[pos_] >= '0' && s_[pos_] <= '9') {
        return fail(err, "JSON_SYNTAX_ERROR", "数值含前导零", pos_, "");
    }
    } else {
      while (!eof() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
    }
    if (!eof() && s_[pos_] == '.') {
      ++pos_;
      if (eof() || s_[pos_] < '0' || s_[pos_] > '9') {
        return fail(err, "JSON_SYNTAX_ERROR", "小数点后缺少数字", pos_, "");
      }
      while (!eof() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
    }
    if (!eof() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
      ++pos_;
      if (!eof() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
      if (eof() || s_[pos_] < '0' || s_[pos_] > '9') {
        return fail(err, "JSON_SYNTAX_ERROR", "指数部分缺少数字", pos_, "");
      }
      while (!eof() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
    }
    const std::string text(s_.substr(start, pos_ - start));
    const double parsed = std::strtod(text.c_str(), nullptr);
    if (!std::isfinite(parsed)) {
      return fail(err, "JSON_NON_FINITE_NUMBER", "数值溢出为无穷：" + text, start, "");
    }
    out = parsed;
    return true;
  }

  /// 解析 JSON 字符串字面量（不含外层引号），产出 UTF-8 文本。
  bool parseString(std::string& out, ParseError& err) {
    if (peek() != '"') {
      return fail(err, "JSON_SYNTAX_ERROR", "期望字符串起始引号", pos_, "");
    }
    const std::size_t start = pos_;
    ++pos_;
    std::string result;
    while (true) {
      if (eof()) {
        return fail(err, "JSON_SYNTAX_ERROR", "字符串未闭合", pos_, "");
      }
      const char c = s_[pos_];
      if (c == '"') {
        ++pos_;
        break;
      }
      if (static_cast<unsigned char>(c) < 0x20) {
        return fail(err, "JSON_SYNTAX_ERROR", "字符串含未转义控制字符", pos_, "");
      }
      if (c != '\\') {
        result.push_back(c);
        ++pos_;
        continue;
      }
      ++pos_;
      if (eof()) {
        return fail(err, "JSON_SYNTAX_ERROR", "转义序列不完整", pos_, "");
      }
      const char e = s_[pos_++];
      if (e == '"' || e == '\\' || e == '/') {
        result.push_back(e);
      } else if (e == 'b') {
        result.push_back('\b');
      } else if (e == 'f') {
        result.push_back('\f');
      } else if (e == 'n') {
        result.push_back('\n');
      } else if (e == 'r') {
        result.push_back('\r');
      } else if (e == 't') {
        result.push_back('\t');
      } else if (e == 'u') {
        unsigned cp = 0;
        if (!parseHex4(cp, err)) return false;
        if (cp >= 0xD800U && cp <= 0xDBFFU) {
          if (!matches("\\u")) {
            return fail(err, "JSON_INVALID_UTF8", "孤立的高位代理项", pos_, "");
        }
          pos_ += 2;
          unsigned lo = 0;
          if (!parseHex4(lo, err)) return false;
          if (lo < 0xDC00U || lo > 0xDFFFU) {
            return fail(err, "JSON_INVALID_UTF8", "代理项未成对", pos_, "");
          }
          cp = 0x10000U + ((cp - 0xD800U) << 10) + (lo - 0xDC00U);
        }
        appendUtf8(result, cp);
      } else {
        return fail(err, "JSON_SYNTAX_ERROR", "非法转义序列", pos_ - 1, "");
      }
    }
    if (result.size() > lim_.maxStringBytes) {
      return fail(err, "JSON_SIZE_LIMIT", "字符串超过大小上限", start, "");
    }
    out = std::move(result);
    return true;
  }

  bool parseHex4(unsigned& out, ParseError& err) {
    if (s_.size() - pos_ < 4) {
      return fail(err, "JSON_SYNTAX_ERROR", "\\u 转义缺少 4 位十六进制", pos_, "");
    }
    unsigned v = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = s_[pos_ + static_cast<std::size_t>(i)];
      const int d = hexVal(c);
      if (d < 0) {
        return fail(err, "JSON_SYNTAX_ERROR", "\\u 转义含非法十六进制字符",
                    pos_ + static_cast<std::size_t>(i), "");
      }
      v = (v << 4) | static_cast<unsigned>(d);
    }
    pos_ += 4;
    out = v;
    return true;
  }

  bool parseArray(JsonValue& out, std::size_t depth, const std::string& pointer, ParseError& err) {
    const std::size_t start = pos_;
    ++pos_;  // '['
    JsonArray items;
    skipWs();
    if (peek() == ']') {
      ++pos_;
      out = JsonValue::makeArray(std::move(items));
      out.setOffset(start);
      return true;
    }
    std::size_t index = 0;
    while (true) {
      const std::string childPtr = pointerJoin(pointer, std::to_string(index));
      JsonValue item;
      if (!parseValue(item, depth + 1, childPtr, err)) return false;
      items.push_back(std::move(item));
      skipWs();
      if (peek() == ',') {
        ++pos_;
        ++index;
        skipWs();
        continue;
      }
      if (peek() == ']') {
        ++pos_;
        break;
      }
      return fail(err, "JSON_SYNTAX_ERROR", "数组元素后期望 ',' 或 ']'", pos_, pointer);
    }
    out = JsonValue::makeArray(std::move(items));
    out.setOffset(start);
    return true;
  }

  bool parseObject(JsonValue& out, std::size_t depth, const std::string& pointer, ParseError& err) {
    const std::size_t start = pos_;
    ++pos_;  // '{'
    JsonObject members;
    skipWs();
    if (peek() == '}') {
      ++pos_;
      out = JsonValue::makeObject(std::move(members));
      out.setOffset(start);
      return true;
    }
    while (true) {
      skipWs();
      std::string key;
      if (!parseString(key, err)) return false;
      for (const auto& kv : members) {
        if (kv.first == key) {
          std::string msg = "重复 key：\"";
        msg += key;
        msg += "\"";
          return fail(err, "JSON_DUPLICATE_KEY", msg, start, pointerJoin(pointer, key));
        }
      }
      skipWs();
      if (peek() != ':') {
        return fail(err, "JSON_SYNTAX_ERROR", "对象成员后期望 ':'", pos_, pointer);
      }
      ++pos_;
      skipWs();
      JsonValue value;
      if (!parseValue(value, depth + 1, pointerJoin(pointer, key), err)) return false;
      members.emplace_back(std::move(key), std::move(value));
      skipWs();
      if (peek() == ',') {
        ++pos_;
        continue;
      }
      if (peek() == '}') {
        ++pos_;
        break;
      }
      return fail(err, "JSON_SYNTAX_ERROR", "对象成员后期望 ',' 或 '}'", pos_, pointer);
    }
    out = JsonValue::makeObject(std::move(members));
    out.setOffset(start);
    return true;
  }
};

}  // namespace

// ---- JsonValue 实现 --------------------------------------------------------

JsonValue JsonValue::makeBool(bool v) noexcept {
  JsonValue j;
  j.type_ = JsonType::kBool;
  j.bool_ = v;
  return j;
}

JsonValue JsonValue::makeNumber(double v) noexcept {
  JsonValue j;
  j.type_ = JsonType::kNumber;
  j.number_ = v;
  return j;
}

JsonValue JsonValue::makeString(std::string v) {
  JsonValue j;
  j.type_ = JsonType::kString;
  j.string_ = std::move(v);
  return j;
}

JsonValue JsonValue::makeArray(JsonArray v) {
  JsonValue j;
  j.type_ = JsonType::kArray;
  j.array_ = std::move(v);
  return j;
}

JsonValue JsonValue::makeObject(JsonObject v) {
  JsonValue j;
  j.type_ = JsonType::kObject;
  j.object_ = std::move(v);
  return j;
}

const JsonValue* JsonValue::find(std::string_view key) const noexcept {
  if (type_ != JsonType::kObject) return nullptr;
  for (const auto& kv : object_) {
    if (kv.first == key) return &kv.second;
  }
  return nullptr;
}

// ---- 解析入口 --------------------------------------------------------------

std::optional<JsonValue> parseStrict(std::string_view text, const Limits& limits, ParseError* err) {
  JsonValue out;
  ParseError local;
  Parser parser(text, limits);
  if (!parser.run(out, local)) {
    if (err != nullptr) *err = local;
    return std::nullopt;
  }
  return out;
}

std::string pointerJoin(std::string_view base, std::string_view token) {
  std::string out(base);
  out.push_back('/');
  // RFC 6901 转义：~ → ~0，/ → ~1
  for (char c : token) {
    if (c == '~') {
      out += "~0";
    } else if (c == '/') {
      out += "~1";
    } else {
      out.push_back(c);
    }
  }
  return out;
}

}  // namespace lbfmt
