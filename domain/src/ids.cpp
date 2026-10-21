// ULID 编解码与 Id 实现。零第三方依赖。
// 布局: 128 bit = 48 bit 毫秒 + 80 bit 随机, 编码为 26 个 Crockford Base32。
// 因 26*5 = 130 > 128, 首字符只承载 3 bit。

#include "loongbones/domain/ids.hpp"

#include <chrono>
#include <cstring>
#include <random>

namespace loongbones {
namespace {

/// 字符 -> 5 bit; 非法返回 -1。直接查表, 避免手写区间换算出错。
int decodeChar(char c) noexcept {
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  if (c == '\0') return -1;
  const char* p = std::strchr(kCrockfordBase32, c);
  return p == nullptr ? -1 : static_cast<int>(p - kCrockfordBase32);
}
std::uint64_t nowMillis() noexcept {
  using namespace std::chrono;
  return static_cast<std::uint64_t>(
      duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

}  // namespace

bool isWellFormedUlid(std::string_view text) noexcept {
  if (text.size() != kUlidLength) return false;
  const int first = decodeChar(text[0]);
  if (first < 0 || first > 7) return false;
  for (std::size_t i = 1; i < text.size(); ++i) {
    if (decodeChar(text[i]) < 0) return false;
  }
  return true;
}
void generateRandom(std::uint64_t& high, std::uint64_t& low) {
  thread_local std::mt19937_64 engine([] {
    std::random_device rd;
    std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
    std::mt19937_64 seeded(seq);
    return seeded;
  }());
  high = engine() & 0xFFFFULL;
  low = engine();
}
std::string encodeUlid(std::uint64_t unixMillis, std::uint64_t randomnessHigh,
                       std::uint64_t randomnessLow) {
  const std::uint64_t hi =
      ((unixMillis & 0xFFFFFFFFFFFFULL) << 16) | (randomnessHigh & 0xFFFFULL);
  const std::uint64_t lo = randomnessLow;

  std::string out(kUlidLength, '0');
  out[0] = kCrockfordBase32[(hi >> 61) & 0x07ULL];
  for (std::size_t i = 1; i <= 12; ++i) {
    out[i] = kCrockfordBase32[(hi >> (61 - 5 * static_cast<std::uint64_t>(i))) & 0x1FULL];
  }  out[13] = kCrockfordBase32[(((hi & 0x01ULL) << 4) | ((lo >> 60) & 0x0FULL)) & 0x1FULL];
  for (std::size_t i = 14; i <= 25; ++i) {
    const std::uint64_t shift = 55 - 5 * static_cast<std::uint64_t>(i - 14);
    out[i] = kCrockfordBase32[(lo >> shift) & 0x1FULL];
  }
  return out;
}
bool decodeUlid(std::string_view text, std::uint64_t& unixMillis, std::uint64_t& randomnessHigh,
                std::uint64_t& randomnessLow) {
  if (!isWellFormedUlid(text)) return false;
  std::uint64_t hi = static_cast<std::uint64_t>(decodeChar(text[0])) & 0x07ULL;
  for (std::size_t i = 1; i <= 12; ++i) {
    hi = (hi << 5) | static_cast<std::uint64_t>(decodeChar(text[i]) & 0x1F);
  }
  // 上面只累积了 63 bit（落在 62..0），而它们应占据 hi 的 bit63..1，故整体左移 1 位。
  hi <<= 1;  // text[13]: 最高位是 hi 的 bit0, 低 4 位是 lo 的 bit63..60;
  // 必须先拆开, 否则连同后续 12 个字符左移 60 位会溢出丢失。
  const unsigned ch13 = static_cast<unsigned>(decodeChar(text[13])) & 0x1FU;
  const std::uint64_t hiBit0 = static_cast<std::uint64_t>((ch13 >> 4) & 0x01U);
  std::uint64_t lo = static_cast<std::uint64_t>(ch13 & 0x0FU);
  for (std::size_t i = 14; i <= 25; ++i) {
    lo = (lo << 5) | static_cast<std::uint64_t>(decodeChar(text[i]) & 0x1F);
  }
  hi |= hiBit0;
  unixMillis = (hi >> 16) & 0xFFFFFFFFFFFFULL;
  randomnessHigh = hi & 0xFFFFULL;
  randomnessLow = lo;
  return true;
}
Id Id::fromString(IdKind kind, std::string_view text) {
  Id id;
  id.kind_ = kind;
  id.value_.assign(text);
  return id;
}

Id Id::generate(IdKind kind) {
  std::uint64_t high = 0;
  std::uint64_t low = 0;
  generateRandom(high, low);
  return Id::fromString(kind, encodeUlid(nowMillis(), high, low));
}

bool Id::isWellFormed() const noexcept { return isWellFormedUlid(value_); }

}  // namespace loongbones