// ID 用例：格式、唯一性、不可变性、不由名称派生。

#include "test_framework.hpp"

#include <set>
#include <string>

#include "loongbones/domain/ids.hpp"

using namespace loongbones;

TEST(Ids, GeneratedIsWellFormed) {
  for (int i = 0; i < 100; ++i) {
    const Id id = Id::generate(IdKind::Node);
    CHECK(id.isWellFormed());
    CHECK_EQ(id.value().size(), kUlidLength);
  }
}

TEST(Ids, GeneratedAreUnique) {
  std::set<std::string> seen;
  for (int i = 0; i < 2000; ++i) {
    const Id id = Id::generate(IdKind::Node);
    CHECK(seen.insert(id.value()).second);
  }
}
TEST(Ids, MalformedRejected) {
  CHECK(!isWellFormedUlid(""));
  CHECK(!isWellFormedUlid("SHORT"));
  CHECK(!isWellFormedUlid(std::string(26, 'U')));
  CHECK(!isWellFormedUlid("IIIIIIIIIIIIIIIIIIIIIIIIII"));
  CHECK(!isWellFormedUlid("88888888888888888888888888"));
}

TEST(Ids, EncodeDecodeRoundTrip) {
  std::uint64_t ms = 0x0123456789ABULL;
  std::uint64_t hi = 0xBEEFULL;
  std::uint64_t lo = 0x0123456789ABCDEFULL;
  const std::string s = encodeUlid(ms, hi, lo);
  CHECK_EQ(s.size(), kUlidLength);
  std::uint64_t dms = 0, dhi = 0, dlo = 0;
  CHECK(decodeUlid(s, dms, dhi, dlo));
  CHECK_EQ(dms, ms);
  CHECK_EQ(dhi, hi);
  CHECK_EQ(dlo, lo);
}
TEST(Ids, KindDistinguishesIds) {
  const Id a = Id::generate(IdKind::Node);
  const Id b = Id::generate(IdKind::ImageAsset);
  CHECK(a != b);
  CHECK(a.kind() != b.kind());
}

TEST(Ids, LowercaseAccepted) {
  const std::string upper = encodeUlid(1, 2, 3);
  std::string lower = upper;
  for (char& c : lower) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  CHECK(isWellFormedUlid(lower));
}