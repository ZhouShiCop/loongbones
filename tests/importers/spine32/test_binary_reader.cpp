// Task 4 B4：Spine 3.2.01 二进制 Reader 的自动化测试。
//
// 【2026-10-21 重写说明】本文件在一次目录整理中被误删（无 git、回收站无记录，
// 无法还原）。当前内容是**依据 Reader 公开 API 与 docs/skeleton/task4_B4.md 记录
// 的布局重新编写**的，用例名沿用原清单以保证覆盖面不缩水，但断言细节可能与
// 原文件不完全一致。若找回原文件，请以原文件为准并保留更强的断言。
//
// 输入全部是内存合成 fixture，不读真实语料（语料扫描由 tools/binary-read-smoke 承担）。

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "loongbones/importers/spine32/binary_reader.hpp"
#include "tests/domain/test_framework.hpp"

using loongbones::importers::spine32::ReadCode;
using loongbones::importers::spine32::ReadResult;
using loongbones::importers::spine32::readSkeletonBinary32;
using loongbones::importers::spine32::toString;

namespace {

// ---- 合成字节写入器（大端，与源格式一致）----
struct W {
  std::vector<std::uint8_t> o;

  void raw(std::uint8_t v) { o.push_back(v); }
  void u8(std::uint8_t v) { o.push_back(v); }
  void boolv(bool v) { o.push_back(v ? 1 : 0); }

  void varint(std::uint32_t v) {
    do {
      std::uint8_t b = static_cast<std::uint8_t>(v & 0x7Fu);
      v >>= 7;
      if (v != 0u) b |= 0x80u;
      o.push_back(b);
    } while (v != 0u);
  }

  void str(const std::string& s) {
    varint(static_cast<std::uint32_t>(s.size() + 1));
    for (char c : s) o.push_back(static_cast<std::uint8_t>(c));
  }
  void strNull() { o.push_back(0); }

  void f32(float v) {
    std::uint32_t raw = 0;
    std::memcpy(&raw, &v, sizeof(raw));
    o.push_back(static_cast<std::uint8_t>(raw >> 24));
    o.push_back(static_cast<std::uint8_t>(raw >> 16));
    o.push_back(static_cast<std::uint8_t>(raw >> 8));
    o.push_back(static_cast<std::uint8_t>(raw));
  }

  void i32(std::int32_t v) {
    const std::uint32_t raw = static_cast<std::uint32_t>(v);
    o.push_back(static_cast<std::uint8_t>(raw >> 24));
    o.push_back(static_cast<std::uint8_t>(raw >> 16));
    o.push_back(static_cast<std::uint8_t>(raw >> 8));
    o.push_back(static_cast<std::uint8_t>(raw));
  }

  std::vector<std::uint8_t> bytes() const { return o; }
};

/// 头：hash / version / width / height / nonessential（+ 可选 imagesPath）。
void writeHeader(W& w, bool nonessential, const std::string& version = "3.2.01") {
  w.str("h");
  w.str(version);
  w.f32(1.f);
  w.f32(1.f);
  w.boolv(nonessential);
  if (nonessential) w.str("images/");
}

/// 一根骨骼：name（+ 可选 parentIndex）+ 8×f32 + 2×bool（+ 非必要颜色）。
void writeBone(W& w, const std::string& name, bool nonessential,
               std::optional<std::uint32_t> parentIndex = std::nullopt) {
  w.str(name);
  if (parentIndex) w.varint(*parentIndex);
  w.f32(0.f); w.f32(0.f); w.f32(0.f);  // rotation, x, y
  w.f32(1.f); w.f32(1.f);              // scaleX, scaleY
  w.f32(0.f); w.f32(0.f); w.f32(0.f);  // shearX, shearY, length
  w.boolv(true);
  w.boolv(true);
  if (nonessential) w.i32(-1);
}

/// region 附件载荷（type=0 之后）：path + 7×f32 + color。
void writeRegion(W& w) {
  w.strNull();
  w.f32(0.f); w.f32(0.f); w.f32(0.f);  // rotation, x, y
  w.f32(1.f); w.f32(1.f);              // scaleX, scaleY
  w.f32(10.f); w.f32(20.f);            // width, height
  w.i32(-1);
}

/// 拼一份最小骨架，default 皮肤里唯一附件的**载荷**由 body 写出。
/// 用于验证 mesh 族（1..5）里已核实的那些布局可以被安全地遍历到 EOF。
template <class F>
std::vector<std::uint8_t> minimalWithAttachment(std::uint8_t typeByte, F body) {
  W w;
  writeHeader(w, false);
  w.varint(1);
  writeBone(w, "root", false);
  w.varint(0); w.varint(0);  // ik, transform
  w.varint(1); w.str("s"); w.varint(0); w.i32(-1); w.str("m"); w.u8(0);
  w.varint(1); w.varint(0); w.varint(1);
  w.str("m"); w.strNull();
  w.u8(typeByte);
  body(w);
  w.varint(0);  // extra skins
  w.varint(0);  // events
  w.varint(0);  // animations
  return w.bytes();
}

void expectFail(const std::vector<std::uint8_t>& bytes, ReadCode want, const std::string& what) {
  const ReadResult r = readSkeletonBinary32(bytes);
  if (r.ok) {
    LBT_FAIL("期望失败但实际通过: " + what);
    return;
  }
  if (r.diagnostics.empty()) {
    LBT_FAIL("失败但没有任何诊断: " + what);
    return;
  }
  CHECK_EQ(static_cast<int>(r.diagnostics.front().code), static_cast<int>(want));
}

}  // namespace

// ---- 合法输入 ----

TEST(BinaryReader, MinimalDocumentAccepted) {
  const ReadResult r = readSkeletonBinary32(minimalWithAttachment(0, [](W& w) { writeRegion(w); }));
  CHECK(r.ok);
  CHECK_EQ(r.skeleton.bones.size(), std::size_t{1});
  CHECK_EQ(r.skeleton.slots.size(), std::size_t{1});
  CHECK_EQ(r.skeleton.skins.size(), std::size_t{1});
  CHECK_EQ(r.skeleton.skins[0].slots.size(), std::size_t{1});
  CHECK_EQ(r.skeleton.skins[0].slots[0].attachments.size(), std::size_t{1});
}

TEST(BinaryReader, EmptyDefaultSkinAccepted) {
  W w;
  writeHeader(w, false);
  w.varint(1);
  writeBone(w, "root", false);
  w.varint(0); w.varint(0);
  w.varint(1); w.str("s"); w.varint(0); w.i32(-1); w.strNull(); w.u8(0);
  w.varint(0);              // 空 default skin
  w.varint(0); w.varint(0); w.varint(0);
  const ReadResult r = readSkeletonBinary32(w.bytes());
  CHECK(r.ok);
  CHECK_EQ(r.skeleton.skins.size(), std::size_t{1});
  CHECK(r.skeleton.skins[0].slots.empty());
}

TEST(BinaryReader, NonessentialImagesPathAccepted) {
  W w;
  writeHeader(w, true);
  w.varint(1);
  writeBone(w, "root", true);  // 非必要时每根骨骼多 4 字节颜色
  w.varint(0); w.varint(0);
  w.varint(0);              // 无插槽
  w.varint(0);              // 空 default skin
  w.varint(0);              // extra skins
  w.varint(0);              // events
  w.varint(0);              // animations
  const ReadResult r = readSkeletonBinary32(w.bytes());
  CHECK(r.ok);
  CHECK(r.skeleton.nonessential);
  CHECK(r.skeleton.imagesPath.has_value());
  CHECK_EQ(r.skeleton.imagesPath.value(), std::string{"images/"});
}

// ---- 头字段 ----

TEST(BinaryReader, HeaderFieldsCaptured) {
  W w;
  w.str("abc123"); w.str("3.2.01"); w.f32(640.f); w.f32(480.f); w.boolv(false);
  w.varint(0);              // bones
  w.varint(0); w.varint(0);  // ik, transform
  w.varint(0);              // slots
  w.varint(0);              // default skin
  w.varint(0); w.varint(0); w.varint(0);
  const ReadResult r = readSkeletonBinary32(w.bytes());
  CHECK(r.ok);
  CHECK(r.skeleton.hash.has_value());
  CHECK_EQ(r.skeleton.hash.value(), std::string{"abc123"});
  CHECK_EQ(r.skeleton.version, std::string{"3.2.01"});
  CHECK_EQ(static_cast<int>(r.skeleton.width), 640);
  CHECK_EQ(static_cast<int>(r.skeleton.height), 480);
  CHECK(!r.skeleton.nonessential);
}

/// hash 字段本身就是一个字符串，不能被当成版本号来校验。
TEST(BinaryReader, HashIsNotTreatedAsVersion) {
  W w;
  w.str("9.9.99"); w.str("3.2.01"); w.f32(1.f); w.f32(1.f); w.boolv(false);
  w.varint(0);
  w.varint(0); w.varint(0);
  w.varint(0);
  w.varint(0);
  w.varint(0); w.varint(0); w.varint(0);
  const ReadResult r = readSkeletonBinary32(w.bytes());
  CHECK(r.ok);
  CHECK_EQ(r.skeleton.hash.value(), std::string{"9.9.99"});
}

TEST(BinaryReader, UnsupportedVersionRejected) {
  W w;
  writeHeader(w, false, "3.3.00");
  w.varint(0);
  w.varint(0); w.varint(0);
  w.varint(0);
  w.varint(0);
  w.varint(0); w.varint(0); w.varint(0);
  expectFail(w.bytes(), ReadCode::kUnsupportedVersion, "版本 3.3.00");
}

// ---- strict EOF ----

TEST(BinaryReader, TrailingBytesRejected) {
  auto bytes = minimalWithAttachment(0, [](W& w) { writeRegion(w); });
  bytes.push_back(0x00);
  expectFail(bytes, ReadCode::kTrailingBytes, "尾部多一个字节");
}

TEST(BinaryReader, TruncatedRejected) {
  auto bytes = minimalWithAttachment(0, [](W& w) { writeRegion(w); });
  bytes.pop_back();
  expectFail(bytes, ReadCode::kTruncated, "截掉最后一个字节");
}

TEST(BinaryReader, EmptyInputRejected) {
  expectFail({}, ReadCode::kTruncated, "空输入");
}

// ---- 非有限浮点 ----

TEST(BinaryReader, NanInBoneRejected) {
  W w;
  writeHeader(w, false);
  w.varint(1);
  w.str("root");
  w.f32(std::numeric_limits<float>::quiet_NaN());  // rotationDeg
  for (int i = 0; i < 7; ++i) w.f32(0.f);
  w.boolv(true); w.boolv(true);
  w.varint(0); w.varint(0);
  w.varint(0);
  w.varint(0);
  w.varint(0); w.varint(0); w.varint(0);
  expectFail(w.bytes(), ReadCode::kNonFiniteFloat, "骨骼 rotationDeg = NaN");
}

TEST(BinaryReader, InfinityInAttachmentRejected) {
  expectFail(
      minimalWithAttachment(0,
                           [](W& w) {
                             w.strNull();
                             w.f32(0.f); w.f32(0.f); w.f32(0.f);
                             w.f32(1.f); w.f32(1.f);
                             w.f32(std::numeric_limits<float>::infinity());  // width
                             w.f32(20.f);
                             w.i32(-1);
                           }),
      ReadCode::kNonFiniteFloat, "附件 width = +Inf");
}

TEST(BinaryReader, NonFiniteDiagnosticCarriesOffsetAndSection) {
  const ReadResult r = readSkeletonBinary32(minimalWithAttachment(0, [](W& w) {
    w.strNull();
    w.f32(0.f); w.f32(0.f); w.f32(0.f);
    w.f32(std::numeric_limits<float>::infinity());  // scaleX
    w.f32(1.f);
    w.f32(10.f); w.f32(20.f);
    w.i32(-1);
  }));
  CHECK(!r.ok);
  CHECK_EQ(r.diagnostics.size(), std::size_t{1});
  const auto& d = r.diagnostics.front();
  CHECK_EQ(static_cast<int>(d.code), static_cast<int>(ReadCode::kNonFiniteFloat));
  CHECK(!d.section.empty());
  CHECK(d.section.find("scaleX") != std::string::npos);
  CHECK(d.offset > 0);
}

// ---- 结构闭合 ----

TEST(BinaryReader, BoneParentForwardReferenceRejected) {
  W w;
  writeHeader(w, false);
  w.varint(2);
  writeBone(w, "root", false);
  writeBone(w, "child", false, 1u);  // 指向自身
  w.varint(0); w.varint(0);
  w.varint(0);
  w.varint(0);
  w.varint(0); w.varint(0); w.varint(0);
  expectFail(w.bytes(), ReadCode::kIndexOutOfRange, "骨骼父索引指向自身");
}

TEST(BinaryReader, BoneParentOutOfRangeRejected) {
  W w;
  writeHeader(w, false);
  w.varint(2);
  writeBone(w, "root", false);
  writeBone(w, "child", false, 9u);  // 越界
  w.varint(0); w.varint(0);
  w.varint(0);
  w.varint(0);
  w.varint(0); w.varint(0); w.varint(0);
  expectFail(w.bytes(), ReadCode::kIndexOutOfRange, "骨骼父索引越界");
}

TEST(BinaryReader, SlotBoneIndexOutOfRangeRejected) {
  W w;
  writeHeader(w, false);
  w.varint(1);
  writeBone(w, "root", false);
  w.varint(0); w.varint(0);
  w.varint(1); w.str("s"); w.varint(7); w.i32(-1); w.strNull(); w.u8(0);
  w.varint(0);
  w.varint(0); w.varint(0); w.varint(0);
  expectFail(w.bytes(), ReadCode::kIndexOutOfRange, "插槽骨骼索引越界");
}

TEST(BinaryReader, UnknownBlendModeRejected) {
  W w;
  writeHeader(w, false);
  w.varint(1);
  writeBone(w, "root", false);
  w.varint(0); w.varint(0);
  w.varint(1); w.str("s"); w.varint(0); w.i32(-1); w.strNull(); w.u8(4);  // blend = 4
  w.varint(0);
  w.varint(0); w.varint(0); w.varint(0);
  expectFail(w.bytes(), ReadCode::kUnknownBlendMode, "混合模式 = 4");
}

// ---- 附件类型 ----

TEST(BinaryReader, UnknownAttachmentTypeRejected) {
  // 6 及以上不在已核实的类型集合内，仍必须明确拒绝而不是猜测字段长度。
  expectFail(minimalWithAttachment(9, [](W&) {}), ReadCode::kUnknownAttachmentType,
             "未知附件类型字节 9");
}

TEST(BinaryReader, BoundingBoxAttachmentTraversed) {
  const ReadResult r = readSkeletonBinary32(minimalWithAttachment(1, [](W& w) {
    w.varint(2);                                   // vertexCount = 2
    for (int i = 0; i < 4; ++i) w.f32(static_cast<float>(i));          // 顶点 ×4（非加权）
  }));
  CHECK(r.ok);
  CHECK_EQ(r.skeleton.unsupported.boundingBox, std::size_t{1});
}

TEST(BinaryReader, MeshAttachmentTraversed) {
  const ReadResult r = readSkeletonBinary32(minimalWithAttachment(2, [](W& w) {
    w.strNull();                                   // path
    w.i32(-1);                                     // color
    w.varint(2);                                   // vertexCount = 2
    for (int i = 0; i < 4; ++i) w.f32(static_cast<float>(i));          // UV ×4
    w.varint(3);                                   // triangleCount = 3
    for (int i = 0; i < 3; ++i) { w.u8(0); w.u8(static_cast<std::uint8_t>(i)); }
    for (int i = 0; i < 4; ++i) w.f32(static_cast<float>(i));          // 顶点 ×4（非加权）
    w.varint(0);                                   // hull
  }));
  CHECK(r.ok);
  CHECK_EQ(r.skeleton.unsupported.mesh, std::size_t{1});
}

TEST(BinaryReader, WeightedMeshAttachmentTraversed) {
  // 加权顶点：每顶点 boneCount(float) + boneCount × (boneIndex, x, y, weight)，
  // boneCount / boneIndex 都**是 float 而不是 varint** —— 这是 sdorica 3.2 的写法。
  const ReadResult r = readSkeletonBinary32(minimalWithAttachment(3, [](W& w) {
    w.strNull();
    w.i32(-1);
    w.varint(2);                                   // vertexCount = 2
    for (int i = 0; i < 4; ++i) w.f32(static_cast<float>(i));          // UV ×4
    w.varint(3);                                   // triangleCount = 3
    for (int i = 0; i < 3; ++i) { w.u8(0); w.u8(static_cast<std::uint8_t>(i)); }
    w.f32(2);                                      // boneCount = 2
    w.f32(0); w.f32(1); w.f32(2); w.f32(0.5f);     // 骨 0
    w.f32(1); w.f32(3); w.f32(4); w.f32(0.5f);     // 骨 1
    w.f32(1);                                      // boneCount = 1
    w.f32(0); w.f32(5); w.f32(6); w.f32(1);        // 骨 0
    w.varint(0);                                   // hull
  }));
  CHECK(r.ok);
  CHECK_EQ(r.skeleton.unsupported.weightedMesh, std::size_t{1});
}

TEST(BinaryReader, WeightedMeshBadBoneCountRejected) {
  // 骨数不是小整数 → 说明字节已错位，必须失败而不是继续往下读。
  expectFail(
      minimalWithAttachment(3, [](W& w) {
        w.strNull(); w.i32(-1); w.varint(1);
        w.f32(0); w.f32(0);        // UV ×2
        w.varint(0);               // triangleCount = 0
        w.f32(2.5f);               // boneCount = 2.5，不是整数
      }),
      ReadCode::kCountLimit, "加权网格的 boneCount 非整数");
}

// ---- 长度前缀边界 ----

TEST(BinaryReader, VarintOverflowRejected) {
  W w;
  writeHeader(w, false);
  for (int i = 0; i < 5; ++i) w.raw(0xFF);  // 5 字节全部带续位 → 超限
  expectFail(w.bytes(), ReadCode::kVarintOverflow, "varint 字节数超限");
}

TEST(BinaryReader, StringLengthLimitRejected) {
  W w;
  writeHeader(w, false);
  w.varint(1);
  w.varint(4098);  // 长度前缀 → 4097 字节内容，超过上限
  expectFail(w.bytes(), ReadCode::kStringTooLong, "字符串长度超过上限");
}

TEST(BinaryReader, ArrayCountLimitRejected) {
  W w;
  writeHeader(w, false);
  w.varint((1u << 20) + 1u);  // bones 数量超过上限
  expectFail(w.bytes(), ReadCode::kCountLimit, "数组长度超过上限");
}

/// 0xFFFFFFFF 是合法的 32 位 varint，不得因为"看起来很大"就被拒。
/// 这里放在事件段的 int 字段上（该字段无取值范围约束）。
TEST(BinaryReader, MaxVarintAccepted) {
  W w;
  writeHeader(w, false);
  w.varint(0);              // bones
  w.varint(0); w.varint(0);  // ik, transform
  w.varint(0);              // slots
  w.varint(0);              // default skin
  w.varint(0);              // extra skins
  w.varint(1);              // events = 1
  w.str("ev");
  w.varint(0xFFFFFFFFu);
  w.f32(0.f);
  w.strNull();
  w.varint(0);              // animations
  const ReadResult r = readSkeletonBinary32(w.bytes());
  CHECK(r.ok);
  CHECK_EQ(r.skeleton.unsupported.events, std::size_t{1});
}

// ---- 失败语义 ----

TEST(BinaryReader, FailureProducesNoPartialSkeleton) {
  W w;
  writeHeader(w, false);
  w.varint(3);                       // 声明 3 根骨骼
  writeBone(w, "root", false);       // 实际只写 1 根后截断
  const ReadResult r = readSkeletonBinary32(w.bytes());
  CHECK(!r.ok);
  CHECK(r.skeleton.bones.empty());
  CHECK(r.skeleton.slots.empty());
  CHECK(r.skeleton.skins.empty());
}

TEST(BinaryReader, ResultCodeStringsAreStable) {
  const ReadCode codes[] = {
      ReadCode::kOk,               ReadCode::kTruncated,       ReadCode::kTrailingBytes,
      ReadCode::kVarintOverflow,   ReadCode::kStringTooLong,   ReadCode::kCountLimit,
      ReadCode::kUnsupportedVersion, ReadCode::kNonFiniteFloat, ReadCode::kUnknownAttachmentType,
      ReadCode::kUnknownTimelineType, ReadCode::kUnknownBlendMode, ReadCode::kIndexOutOfRange,
      ReadCode::kInvalidUtf8};
  // 每个码都要有稳定的非空字符串（程序逻辑按枚举判断，但日志/报告要靠它可读）。
  std::vector<std::string> seen;
  for (ReadCode c : codes) {
    const char* s = toString(c);
    CHECK(s != nullptr);
    CHECK(std::string(s).size() >= 2);
    seen.emplace_back(s);
  }
  // 不同码不得共用同一个字符串，否则诊断会被误读。
  std::sort(seen.begin(), seen.end());
  CHECK(std::adjacent_find(seen.begin(), seen.end()) == seen.end());
  CHECK_EQ(seen.front(), std::string{"COUNT_LIMIT"});
}
