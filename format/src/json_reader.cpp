#include "loongbones/format/json_reader.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#include "rapidjson/reader.h"

namespace loongbones {
namespace format {
namespace {

// 可 Tell() 的输入流。RapidJSON 的 MemoryStream 不支持 Tell，
// 而字节偏移是 RFC §4.6 要求的可定位信息之一。
class CountedStream {
 public:
  using Ch = char;

  CountedStream(const char* data, std::size_t size) : data_(data), size_(size) {}

  Ch Peek() const { return pos_ < size_ ? data_[pos_] : '\0'; }
  Ch Take() { return pos_ < size_ ? data_[pos_++] : '\0'; }
  std::size_t Tell() const { return pos_; }

  // 以下三个是 RapidJSON Stream 概念的组成部分：模板实例化时
  // kParseInsituFlag 分支会被编译（即使运行时不走），故必须存在。
  // 我们从不使用 insitu 解析，因此它们运行时不会被调用。
  Ch* PutBegin() { return nullptr; }
  void Put(Ch) {}
  std::size_t PutEnd(Ch*) { return 0; }
  void Flush() {}

 private:
  const char* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

// 解析期使用的 RapidJSON 标志。
// **不得**加入：kParseNanAndInfFlag（会接受 NaN/Infinity）、
// kParseCommentsFlag、kParseTrailingCommasFlag。
// kParseNumbersAsStringsFlag 让数值走 RawNumber 回调，从而拿到原始字面量。
constexpr unsigned kParseFlags =
    rapidjson::kParseNumbersAsStringsFlag | rapidjson::kParseValidateEncodingFlag;

bool isWhitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// ---- 数值字面量的"等长占位"机制（RFC §6.6）----
//
// RapidJSON 的 ParseNumber 即使开了 kParseNumbersAsStringsFlag，也仍会在
// reader.h 里做量级检查（exp > 308 → kParseErrorNumberTooBig）。结果是 `1e400`
// 这类**语法合法**的字面量在解析期就被拒，连原始文本都拿不到 —— 而 §6.6 要求
// 未知字段里的 `1e400` 必须原样保留且**不是错误**。
//
// 对策：把被拒的字面量**等长**替换为 `0` + 空格（仍是合法 JSON），重新解析；
// 按**字节偏移**记录原文，在 RawNumber 回调里还原。
// 等长替换是关键：所有后续字节偏移不变，诊断位置仍然准确。
//
// 只存 (offset, length)：原文一直可从调用方的**入参文本**按同一偏移取回
// （占位只改工作副本，且长度不变），故不必为每个占位复制一份字面量。
struct NumberPatch {
  std::size_t offset = 0;
  std::size_t length = 0;
};

// 从 offset 扫描一个 RFC 8259 数值字面量，返回其长度（0 表示此处不是数值）
std::size_t scanNumberLiteral(const std::string& text, std::size_t offset) {
  std::size_t i = offset;
  if (i < text.size() && text[i] == '-') ++i;
  if (i < text.size() && text[i] == '0') {
    ++i;
  } else {
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') ++i;
  }
  if (i < text.size() && text[i] == '.') {
    ++i;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') ++i;
  }
  if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
    ++i;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) ++i;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') ++i;
  }
  return i - offset;
}

// ---- "RapidJSON 会不会拒绝这个字面量"的保守判据 ----
//
// vendored RapidJSON v1.1.0 在 reader.h 的 ParseNumber() 里只有两处会报
// kParseErrorNumberTooBig：
//   · 整数部分过长：double 累积到 d >= 1.7976931348623157e307 后还要进位；
//   · 正指数过大：exp > 308 - expFrac（expFrac 最多约为小数位数）。
// 负指数不会报错（下溢为 0），故下面只对**非负**指数判越界。
//
// 本判据是 RapidJSON 实际拒绝集合的**超集**：把尚可解析的字面量也占位掉没有任何
// 可见影响 —— 占位是等长的，且原文按字节偏移原样还原（RFC §6.6）。
// 判据刻意保持便宜（一次线性扫字面量），以便一次性处理整份文档。
constexpr std::size_t kDoubleMaxDecimalExponent = 308;

bool numberExceedsDoubleRange(const std::string& text, std::size_t offset, std::size_t length) {
  const std::size_t end = offset + length;
  std::size_t i = offset;
  if (i < end && text[i] == '-') ++i;

  // 整数部分：只数有效数字（前导零不抬高量级）
  std::size_t significantDigits = 0;
  while (i < end && text[i] >= '0' && text[i] <= '9') {
    if (text[i] != '0' || significantDigits != 0) ++significantDigits;
    ++i;
  }
  if (significantDigits > kDoubleMaxDecimalExponent) return true;

  if (i < end && text[i] == '.') {
    ++i;
    while (i < end && text[i] >= '0' && text[i] <= '9') ++i;
  }
  if (i < end && (text[i] == 'e' || text[i] == 'E')) {
    ++i;
    bool negative = false;
    if (i < end && (text[i] == '+' || text[i] == '-')) {
      negative = (text[i] == '-');
      ++i;
    }
    // 指数位数可能极多：一旦超过阈值即停止累加，避免无意义的大数运算
    std::size_t exponent = 0;
    while (i < end && text[i] >= '0' && text[i] <= '9') {
      if (exponent <= kDoubleMaxDecimalExponent) {
        exponent = exponent * 10 + static_cast<std::size_t>(text[i] - '0');
      }
      ++i;
    }
    if (!negative && exponent > kDoubleMaxDecimalExponent) return true;
  }
  return false;
}

// 一次性批量占位：扫描**字符串之外**的数值字面量，把所有可能被 RapidJSON 判为
// kParseErrorNumberTooBig 的字面量等长替换为 `0` + 空格，并登记占位表。
//
// 有了它，任意数量的越界数值都只需 **一次** 重解析，避免"遇到一个就把整份文档
// 重新解析一遍"的 O(越界数量 x 文档字节数) 行为（Task 3 Back §2.2）。
//
// 长度超过 maxStringBytes 的字面量**故意不占位**：它必须留给 RawNumber 的长度
// 检查（或 NumberTooBig 分支里的原始长度检查）带着完整位置报 kJsonSizeLimit，
// 否则占位后 RawNumber 只会看到 `0`，长度限制就被绕过了（Task 3 Back §2.1）。
void applyNumberPlaceholders(std::string& working, const ReaderLimits& limits,
                             std::vector<NumberPatch>& patches) {
  std::size_t i = 0;
  while (i < working.size()) {
    const char c = working[i];
    if (c == '"') {
      // 整个字符串原样跳过：字符串里的数字不是数值字面量，绝不能被占位
      ++i;
      while (i < working.size()) {
        if (working[i] == '\\') {
          i += 2;
          continue;
        }
        if (working[i] == '"') {
          ++i;
          break;
        }
        ++i;
      }
      continue;
    }
    // 合法 JSON 里，字符串之外的 '-' 与数字只可能出现在数值字面量中
    if (c == '-' || (c >= '0' && c <= '9')) {
      const std::size_t length = scanNumberLiteral(working, i);
      if (length == 0) {
        ++i;
        continue;
      }
      if (length <= limits.maxStringBytes && numberExceedsDoubleRange(working, i, length)) {
        patches.push_back(NumberPatch{i, length});
        for (std::size_t k = 0; k < length; ++k) working[i + k] = (k == 0) ? '0' : ' ';
      }
      i += length;
      continue;
    }
    ++i;
  }
}

// 在 offset 处是否看起来是 NaN / Infinity 字面量。
// 这两者是**非标准字面量**（RFC 8259 不允许），与 1e400（合法但越界）是两类问题，
// 故分属 kJsonNonFiniteNumber 与 kJsonNumberOutOfRange。
bool looksLikeNonFinite(const std::string& text, std::size_t offset) {
  std::size_t i = offset;
  while (i < text.size() && isWhitespace(text[i])) ++i;
  static const char* const kTokens[] = {"-Infinity", "Infinity", "NaN", "-inf",
                                        "inf",       "nan",      "-INF", "INF",
                                        "NAN"};
  for (const char* token : kTokens) {
    const std::size_t n = std::strlen(token);
    if (text.compare(i, n, token) == 0) return true;
  }
  return false;
}

// SAX 事件 -> JsonValue 树。
class TreeBuilder {
 public:
  /// @param source 原始入参文本：占位只改工作副本且长度不变，故可按同一偏移取回原文
  TreeBuilder(const ReaderLimits& limits, const CountedStream* stream,
              const std::vector<NumberPatch>& patches, const std::string* source)
      : limits_(limits), stream_(stream), patches_(patches), source_(source) {}

  bool Null() {
    JsonValue v;
    return pushValue(std::move(v));
  }

  bool Bool(bool b) {
    JsonValue v;
    v.type = JsonType::kBool;
    v.boolValue = b;
    return pushValue(std::move(v));
  }

  // 以下各数值回调在 kParseNumbersAsStringsFlag 下不会被调用，
  // 但 RapidJSON 的 Handler 概念要求它们存在。保留为「转为字面量文本」，
  // 以防未来标志变化导致数值被静默丢弃。
  bool Int(int i) { return pushNumberFromIntegral(i); }
  bool Uint(unsigned u) { return pushNumberFromIntegral(u); }
  bool Int64(std::int64_t i) { return pushNumberFromIntegral(i); }
  bool Uint64(std::uint64_t u) { return pushNumberFromIntegral(u); }
  bool Double(double d) {
    JsonValue v;
    v.type = JsonType::kNumber;
    v.numberRaw = formatShortestDouble(d);
    return pushValue(std::move(v));
  }

  bool RawNumber(const char* str, rapidjson::SizeType length, bool /*copy*/) {
    if (length > limits_.maxStringBytes) return failSize("数值字面量过长");
    JsonValue v;
    v.type = JsonType::kNumber;
    // RFC §6.6：只保留原始字面量，不做任何类型化转换
    v.numberRaw.assign(str, length);
    // 若该数值落在"等长占位"的偏移上，还原被 RapidJSON 拒收的**原始**字面量。
    // 已知字段随后按其 schema 类型解析该字面量并可能报 kJsonNumberOutOfRange；
    // 未知字段则仅持有 raw，不做转换，故不存在越界（§6.6.3）。
    //
    // 占位表按 offset 升序，而 SAX 回调的偏移单调递增，故用游标只进不退 ——
    // 每个占位的还原是摊还 O(1)，与占位数量无关。
    if (stream_ != nullptr && source_ != nullptr) {
      const std::size_t end = stream_->Tell();
      if (end >= static_cast<std::size_t>(length)) {
        const std::size_t start = end - static_cast<std::size_t>(length);
        while (cursor_ < patches_.size() && patches_[cursor_].offset < start) ++cursor_;
        if (cursor_ < patches_.size() && patches_[cursor_].offset == start &&
            start + patches_[cursor_].length <= source_->size()) {
          v.numberRaw.assign(*source_, start, patches_[cursor_].length);
        }
      }
    }
    return pushValue(std::move(v));
  }

  bool String(const char* str, rapidjson::SizeType length, bool /*copy*/) {
    if (length > limits_.maxStringBytes) return failSize("字符串过长");
    JsonValue v;
    v.type = JsonType::kString;
    v.stringValue.assign(str, length);
    return pushValue(std::move(v));
  }

  bool StartObject() { return beginContainer(true); }
  bool StartArray() { return beginContainer(false); }

  bool Key(const char* str, rapidjson::SizeType length, bool /*copy*/) {
    if (length > limits_.maxStringBytes) return failSize("键名过长");
    Frame& frame = frames_.back();
    frame.pendingKey.assign(str, length);
    // RFC §4.2：重复 key 必须拒绝，不得静默取后者
    for (const auto& member : frame.value.members) {
      if (member.first == frame.pendingKey) {
        return fail(FormatDiagnosticCode::kJsonDuplicateKey,
                    "对象内出现重复 key：" + frame.pendingKey,
                    pointerForMemberOfCurrentFrame());
      }
    }
    return true;
  }

  bool EndObject(rapidjson::SizeType /*memberCount*/) { return endContainer(); }
  bool EndArray(rapidjson::SizeType /*elementCount*/) { return endContainer(); }

  [[nodiscard]] bool failed() const noexcept { return failed_; }
  [[nodiscard]] bool completed() const noexcept { return completed_; }

  /// 解析中断处"正待写入的成员"的 Pointer，供解析期诊断定位（RFC §4.6）。
  [[nodiscard]] std::string pendingPointer() const {
    if (frames_.empty()) return std::string();
    return pointerForMemberOfCurrentFrame();
  }

  FormatDiagnostic takeDiagnostic() { return std::move(diag_); }
  JsonValue takeRoot() { return std::move(root_); }

 private:
  struct Frame {
    JsonValue value;
    std::string pendingKey;
    bool isObject = false;
    std::size_t arrayIndex = 0;
  };

  template <typename T>
  bool pushNumberFromIntegral(T value) {
    JsonValue v;
    v.type = JsonType::kNumber;
    v.numberRaw = std::to_string(value);
    return pushValue(std::move(v));
  }

  bool beginContainer(bool isObject) {
    if (frames_.size() + 1 > limits_.maxDepth) {
      return fail(FormatDiagnosticCode::kJsonDepthLimit, "嵌套深度超过上限", currentPointer());
    }
    if (!frames_.empty()) {
      // 新容器的段名由父容器决定
      Frame& parent = frames_.back();
      if (parent.isObject) {
        segments_.push_back(escapePointerSegment(parent.pendingKey));
      } else {
        segments_.push_back(std::to_string(parent.arrayIndex));
      }
    }
    Frame frame;
    frame.isObject = isObject;
    frame.value.type = isObject ? JsonType::kObject : JsonType::kArray;
    frames_.push_back(std::move(frame));
    return true;
  }

  bool endContainer() {
    JsonValue container = std::move(frames_.back().value);
    frames_.pop_back();
    if (!segments_.empty()) segments_.pop_back();
    return pushValue(std::move(container));
  }

  bool pushValue(JsonValue v) {
    if (frames_.empty()) {
      if (completed_) {
        return fail(FormatDiagnosticCode::kJsonSyntaxError, "文档含有多个根值", "");
      }
      completed_ = true;
      root_ = std::move(v);
      return true;
    }
    Frame& frame = frames_.back();
    if (frame.isObject) {
      frame.value.members.emplace_back(std::move(frame.pendingKey), std::move(v));
      frame.pendingKey.clear();
    } else {
      frame.value.array.push_back(std::move(v));
      ++frame.arrayIndex;
    }
    return true;
  }

  bool fail(FormatDiagnosticCode code, std::string message, std::string location) {
    failed_ = true;
    diag_ = FormatDiagnostic(code, std::move(message), std::move(location));
    return false;
  }

  bool failSize(const char* what) {
    return fail(FormatDiagnosticCode::kJsonSizeLimit, what, currentPointer());
  }

  // 当前容器的 JSON Pointer。根容器为 ""（文档级）。
  [[nodiscard]] std::string currentPointer() const {
    std::string p;
    for (const auto& seg : segments_) {
      p += '/';
      p += seg;
    }
    return p;
  }

  // 当前容器内「正待写入的成员」的 Pointer。
  [[nodiscard]] std::string pointerForMemberOfCurrentFrame() const {
    std::string p = currentPointer();
    const Frame& frame = frames_.back();
    p += '/';
    if (frame.isObject) {
      p += escapePointerSegment(frame.pendingKey);
    } else {
      p += std::to_string(frame.arrayIndex);
    }
    return p;
  }

  const ReaderLimits& limits_;
  const CountedStream* stream_;
  const std::vector<NumberPatch>& patches_;
  const std::string* source_;
  std::size_t cursor_ = 0;
  std::vector<Frame> frames_;
  std::vector<std::string> segments_;
  JsonValue root_;
  FormatDiagnostic diag_;
  bool failed_ = false;
  bool completed_ = false;
};

}  // namespace

JsonParseResult parseJson(const std::string& text, const ReaderLimits& limits) {
  JsonParseResult out;

  // RFC §4.3：拒绝 UTF-8 BOM
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
    out.diag = FormatDiagnostic::withOffset(FormatDiagnosticCode::kJsonInvalidUtf8,
                                           "存在 UTF-8 BOM（EF BB BF）", "", 0);
    return out;
  }

  // RFC §4.4：文档大小上限
  if (text.size() > limits.maxDocBytes) {
    out.diag = FormatDiagnostic(FormatDiagnosticCode::kJsonSizeLimit, "文档字节数超过上限", "");
    return out;
  }

  // 工作副本：遇到"量级过大"的字面量时做**等长**占位替换后重解析（RFC §6.6）
  std::string working = text;
  std::vector<NumberPatch> patches;

  // 重解析次数**不再有固定常量上限**（Task 3 Back §2.2）：每轮至少把一个此前未
  // 占位的字面量占位，而字面量互不相交、每个至少占 1 字节，故轮数天然不可能超过
  // 文档字节数。上限因此完全由 §4.4 的 maxDocBytes 约束，不需要新的持久化契约。
  const std::size_t maxAttempts = working.size() + 2;
  bool batchPlaceholdersApplied = false;

  for (std::size_t attempt = 0; attempt < maxAttempts; ++attempt) {
    CountedStream stream(working.data(), working.size());
    TreeBuilder builder(limits, &stream, patches, &text);
    rapidjson::Reader reader;
    const rapidjson::ParseResult result = reader.Parse<kParseFlags>(stream, builder);

    if (!result.IsError()) {
      if (!builder.completed()) {
        out.diag = FormatDiagnostic(FormatDiagnosticCode::kJsonSyntaxError, "文档为空", "");
        return out;
      }
      out.ok = true;
      out.root = builder.takeRoot();
      return out;
    }

    // 语法合法但量级越界（如 1e400）：RapidJSON 连字面量都不给。
    // 占位后重解析，使字面量得以保留 —— 未知字段据此原样回写（§6.6.3）。
    if (result.Code() == rapidjson::kParseErrorNumberTooBig) {
      const std::size_t offset = result.Offset();
      const std::size_t length = scanNumberLiteral(working, offset);
      if (length == 0) {
        out.diag = FormatDiagnostic(FormatDiagnosticCode::kJsonSyntaxError,
                                    "数值字面量无法定位", "");
        out.diag.offset = result.Offset();
        out.diag.hasOffset = true;
        return out;
      }
      // Task 3 Back §2.1：**占位之前**用 working 里尚未替换的**原始** token 长度
      // 执行 Reader 上限检查。占位一旦生效，重解析时 RawNumber 只收到 `0`，
      // 那里的长度检查就再也看不到原始字面量 —— 故检查必须发生在这一步。
      if (length > limits.maxStringBytes) {
        out.diag = FormatDiagnostic::withOffset(FormatDiagnosticCode::kJsonSizeLimit,
                                                "数值字面量过长", builder.pendingPointer(),
                                                offset);
        return out;
      }

      if (!batchPlaceholdersApplied) {
        // 批量占位：一次覆盖整份文档里所有可能越界的字面量，之后只需再解析一次。
        // 正常路径下（含成千上万个越界数值）总共只会有两次解析。
        batchPlaceholdersApplied = true;
        applyNumberPlaceholders(working, limits, patches);
        continue;
      }

      // 批量占位的判据是 RapidJSON 拒绝集合的超集；走到这里说明判据漏了某一类
      // 字面量。退化为单点占位，保证仍能推进（插入时保持 offset 升序）。
      const auto at = std::lower_bound(
          patches.begin(), patches.end(), offset,
          [](const NumberPatch& patch, std::size_t off) { return patch.offset < off; });
      patches.insert(at, NumberPatch{offset, length});
      for (std::size_t k = 0; k < length; ++k) {
        working[offset + k] = (k == 0) ? '0' : ' ';
      }
      continue;
    }

    if (builder.failed()) {
      out.diag = builder.takeDiagnostic();
    } else {
      FormatDiagnosticCode code = FormatDiagnosticCode::kJsonSyntaxError;
      if (result.Code() == rapidjson::kParseErrorValueInvalid &&
          looksLikeNonFinite(working, result.Offset())) {
        code = FormatDiagnosticCode::kJsonNonFiniteNumber;
      }
      if (result.Code() == rapidjson::kParseErrorStringInvalidEncoding) {
        code = FormatDiagnosticCode::kJsonInvalidUtf8;
      }
      out.diag = FormatDiagnostic(code, "JSON 不符合 RFC 8259", "");
    }
    out.diag.offset = result.Offset();
    out.diag.hasOffset = true;
    return out;
  }

  // 防御性出口：占位互不重叠且各占至少 1 字节，轮数不可能超过字节数，故正常不可达。
  // 真被触发说明判据有漏，按 §4.4 的大小限制语义回报，不伪装成语法错误。
  out.diag = FormatDiagnostic(FormatDiagnosticCode::kJsonSizeLimit,
                              "文档中超出 double 量级的数值字面量过多", "");
  return out;
}

}  // namespace format
}  // namespace loongbones
