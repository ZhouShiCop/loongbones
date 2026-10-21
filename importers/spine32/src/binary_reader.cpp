// Spine 3.2.01 二进制 Reader 实现（Task 4 B4）
//
// 布局（独立字节走查结论，样本 02 精确到 EOF）：
//
//   header:      hash(string) version(string) width(f32) height(f32) nonessential(u8)
//                [imagesPath(string)]            // 仅 nonessential
//   bones:       count(varint) per bone: name(string) [parentIndex(varint, i>0)]
//                rotation,x,y,scaleX,scaleY,shearX,shearY,length (8×f32)
//                inheritScale(u8) inheritRotation(u8)
//   ik:          count(varint) …                 // 本任务只计数，参数跳过
//   transform:   count(varint) …                 // 同上
//   slots:       count(varint) per slot: name(string) boneIndex(varint)
//                color(i32 0xRRGGBBAA) attachment(string?) blend(u8)
//   skins:       default skin（无名字）→ extraSkinCount(varint) → 每个 (name + 内容)
//      skin:     slotCount(varint) per slot: slotIndex(varint) attCount(varint)
//                per att: key(string) name(string?) type(u8) <类型专属>
//      region:   path(string?) rotation,x,y,scaleX,scaleY,width,height (7×f32) color(i32)
//   events:      count(varint) per event: name int float string
//   animations:  count(varint) per animation:
//                name(string)
//                slotCount(varint) per: slotIndex(varint) timelineCount(varint)
//                   per tl: type(u8) frameCount(varint) frames + 帧间 curve 字节
//                boneCount(varint) per: boneIndex(varint) timelineCount(varint) …
//                ikCount(varint) transformCount(varint) deformCount(varint)
//                drawOrderCount(varint) eventCount(varint)
//
// curve 字节：仅"可插值"时间线（rotate/translate/scale/shear/color/deform/ik/transform）
// 在**非最后一帧之后**写一个字节：0=linear 1=stepped 2=bezier(4×f32)。
// attachment / drawOrder / event 时间线不写 curve。

#include "loongbones/importers/spine32/binary_reader.hpp"

#include <cmath>
#include <cstring>

namespace loongbones::importers::spine32 {
namespace {

/// 单个顶点允许的骨数上限。用于把"字节已错位"和"真的有很多骨骼"区分开：
/// 错位时读到的 float 往往是一个很大的值，这里直接判失败而不是继续读下去。
inline constexpr float kMaxBonesPerVertex = 64.f;

// ---- 时间线类型（源枚举；只用于跳到 EOF 与计数）----
enum TimelineKind : std::uint8_t {
  kTlRotate = 0,
  kTlTranslate = 1,
  kTlScale = 2,
  kTlShear = 3,
  kTlAttachment = 4,
  kTlColor = 5,
  kTlDeform = 6,
  kTlIk = 7,
  kTlDrawOrder = 8,
  kTlTransform = 9,
  kTlEvent = 10,
};

/// 这些时间线写 curve 字节；其余（attachment/drawOrder/event）不写。
bool timelineHasCurve(std::uint8_t kind) {
  switch (kind) {
    case kTlRotate:
    case kTlTranslate:
    case kTlScale:
    case kTlShear:
    case kTlColor:
    case kTlDeform:
    case kTlIk:
    case kTlTransform:
      return true;
    default:
      return false;
  }
}

class Reader {
 public:
  Reader(const std::vector<std::uint8_t>& bytes, std::vector<ReadDiagnostic>& out,
         UnsupportedCounts& unsupported)
      : b_(bytes), out_(out), unsupported_(unsupported) {}

  bool run(SourceSkeleton& sk) {
    readHeader(sk);
    if (!ok()) return false;
    readBones(sk);
    if (!ok()) return false;
    readConstraintCounts(sk);
    if (!ok()) return false;
    readSlots(sk);
    if (!ok()) return false;
    readSkins(sk);
    if (!ok()) return false;
    readEvents(sk);
    if (!ok()) return false;
    readAnimations(sk);
    if (!ok()) return false;
    if (pos_ != b_.size()) {
      fail(ReadCode::kTrailingBytes, "解析完成后仍有尾随字节（strict EOF）", "", pos_);
      return false;
    }
    return true;
  }

 private:
  const std::vector<std::uint8_t>& b_;
  std::vector<ReadDiagnostic>& out_;
  UnsupportedCounts& unsupported_;
  bool nonessential_ = false;
  std::size_t pos_ = 0;

  bool ok() const { return out_.empty(); }

  void fail(ReadCode code, std::string message, std::string section, std::size_t offset) {
    // 只记第一条：后续报错多半是同一处损坏的连带结果，堆砌反而掩盖根因。
    if (out_.empty()) out_.push_back({code, std::move(message), std::move(section), offset});
  }

  bool need(std::size_t n, const char* section) {
    if (pos_ + n > b_.size()) {
      fail(ReadCode::kTruncated, "剩余字节不足", section, pos_);
      return false;
    }
    return true;
  }

  // ---- 原语 ----

  std::uint8_t u8(const char* section) {
    if (!need(1, section)) return 0;
    return b_[pos_++];
  }

  bool boolAt(const char* section) { return u8(section) != 0; }

  /// 大端 float32；读取**立即**做有限性检查（field-mapping §12）。
  double f32(const char* section) {
    if (!need(4, section)) return 0.0;
    std::uint32_t raw = (static_cast<std::uint32_t>(b_[pos_]) << 24) |
                        (static_cast<std::uint32_t>(b_[pos_ + 1]) << 16) |
                        (static_cast<std::uint32_t>(b_[pos_ + 2]) << 8) |
                        static_cast<std::uint32_t>(b_[pos_ + 3]);
    pos_ += 4;
    float v = 0.0f;
    std::memcpy(&v, &raw, sizeof(v));
    if (!std::isfinite(v)) {
      fail(ReadCode::kNonFiniteFloat, "float32 为 NaN 或 ±Inf", section, pos_ - 4);
      return 0.0;
    }
    return static_cast<double>(v);
  }

  std::int32_t i32(const char* section) {
    if (!need(4, section)) return 0;
    const std::uint32_t raw = (static_cast<std::uint32_t>(b_[pos_]) << 24) |
                              (static_cast<std::uint32_t>(b_[pos_ + 1]) << 16) |
                              (static_cast<std::uint32_t>(b_[pos_ + 2]) << 8) |
                              static_cast<std::uint32_t>(b_[pos_ + 3]);
    pos_ += 4;
    return static_cast<std::int32_t>(raw);
  }

  /// 非负 LEB128 varint。超过 32 位或字节数超限即失败（防恶意长度前缀）。
  std::uint32_t varint(const char* section) {
    std::uint64_t result = 0;
    for (std::size_t i = 0; i < kMaxVarintBytes; ++i) {
      if (!need(1, section)) return 0;
      const std::uint8_t byte = b_[pos_++];
      result |= static_cast<std::uint64_t>(byte & 0x7Fu) << (7 * i);
      if ((byte & 0x80u) == 0u) {
        if (result > 0xFFFFFFFFu) {
          fail(ReadCode::kVarintOverflow, "varint 超过 32 位", section, pos_);
          return 0;
        }
        return static_cast<std::uint32_t>(result);
      }
    }
    fail(ReadCode::kVarintOverflow, "varint 字节数超限", section, pos_);
    return 0;
  }

  /// 长度前缀字符串：0 = null；n = 长度 + 1（故空串写作 1）。
  bool readString(std::optional<std::string>& out, const char* section) {
    const std::uint32_t n = varint(section);
    if (!ok()) return false;
    if (n == 0) {
      out.reset();
      return true;
    }
    const std::size_t len = n - 1;
    if (len > kMaxStringBytes) {
      fail(ReadCode::kStringTooLong, "字符串长度超过上限", section, pos_);
      return false;
    }
    if (!need(len, section)) return false;
    std::string s(reinterpret_cast<const char*>(b_.data() + pos_), len);
    pos_ += len;
    out = std::move(s);
    return true;
  }

  std::string readStringRequired(const char* section) {
    std::optional<std::string> s;
    readString(s, section);
    return s.value_or(std::string{});
  }

  /// 数组长度：上限检查，避免用超长 count 触发巨量分配。
  std::size_t count(const char* section) {
    const std::uint32_t n = varint(section);
    if (!ok()) return 0;
    if (n > kMaxArrayCount) {
      fail(ReadCode::kCountLimit, "数组长度超过上限", section, pos_);
      return 0;
    }
    return static_cast<std::size_t>(n);
  }

  // ---- 各段 ----

  void readHeader(SourceSkeleton& sk) {
    readString(sk.hash, "header/hash");
    if (!ok()) return;
    sk.version = readStringRequired("header/version");
    if (!ok()) return;
    if (sk.version != kAcceptedVersion) {
      fail(ReadCode::kUnsupportedVersion, "头版本不是 3.2.01", "header/version", pos_);
      return;
    }
    sk.width = f32("header/width");
    sk.height = f32("header/height");
    if (!ok()) return;
    sk.nonessential = boolAt("header/nonessential");
    nonessential_ = sk.nonessential;
    if (!ok()) return;
    if (sk.nonessential) {
      readString(sk.imagesPath, "header/imagesPath");
    }
  }

  void readBones(SourceSkeleton& sk) {
    const std::size_t n = count("bones");
    if (!ok()) return;
    sk.bones.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
      const std::string base = "bones/" + std::to_string(i) + "/";
      SourceBone& bone = sk.bones[i];
      bone.name = readStringRequired((base + "name").c_str());
      if (!ok()) return;
      if (i > 0) {
        const std::uint32_t raw = varint((base + "parentIndex").c_str());
        if (!ok()) return;
        // 源里 -1 表示根。语料中根骨恒为第 0 根（该根不写 parentIndex 字段），
        // 这里仍按 32 位无符号 varint 接受，越界与否交由结构校验判定。
        bone.parentIndex = static_cast<std::int32_t>(raw);
      } else {
        bone.parentIndex = -1;
      }
      bone.rotationDeg = f32((base + "rotationDeg").c_str());
      bone.x = f32((base + "x").c_str());
      bone.y = f32((base + "y").c_str());
      bone.scaleX = f32((base + "scaleX").c_str());
      bone.scaleY = f32((base + "scaleY").c_str());
      bone.shearXDeg = f32((base + "shearXDeg").c_str());
      bone.shearYDeg = f32((base + "shearYDeg").c_str());
      bone.length = f32((base + "length").c_str());
      if (!ok()) return;
      bone.inheritScale = boolAt((base + "inheritScale").c_str());
      bone.inheritRotation = boolAt((base + "inheritRotation").c_str());
      if (!ok()) return;
      // 非必要时每根骨骼还跟一个 4 字节颜色（本任务不消费，但必须跳过）。
      // 漏掉它会让整个骨架段错位 —— e0002s3 / h0270s5 / h0270s5_whale / h0270s5_miniwhale
      // 这 4 个 nonessential=1 的文件正是因此在 bones/1/name 处炸出 STRING_TOO_LONG。
      if (nonessential_) {
        i32((base + "color").c_str());
        if (!ok()) return;
      }
    }
    // 结构闭合：语料实测越界 0、前向引用 0、自环 0。这里按同一契约拒绝。
    for (std::size_t i = 0; i < sk.bones.size(); ++i) {
      const int parent = sk.bones[i].parentIndex;
      if (parent < -1 || static_cast<std::size_t>(parent) >= i) {
        // parent == -1 是根；0..i-1 是合法父引用；其余（含自环 i、前向 >i、越界）全拒。
        if (parent != -1) {
          fail(ReadCode::kIndexOutOfRange, "骨骼父索引越界、指向自身或前向引用",
               "bones/" + std::to_string(i) + "/parentIndex", pos_);
          return;
        }
      }
    }
  }

  /// IK / transform 约束段：本任务只**计数**，参数按字节跳过。
  void readConstraintCounts(SourceSkeleton& sk) {
    sk.unsupported.ikConstraints = count("ik");
    if (!ok()) return;
    for (std::size_t i = 0; i < sk.unsupported.ikConstraints; ++i) {
      const std::string base = "ik/" + std::to_string(i) + "/";
      readStringRequired((base + "name").c_str());
      if (!ok()) return;
      const std::size_t boneCount = count((base + "bones").c_str());
      if (!ok()) return;
      for (std::size_t j = 0; j < boneCount; ++j) varint((base + "bones").c_str());
      if (!ok()) return;
      varint((base + "target").c_str());
      if (!ok()) return;
      f32((base + "mix").c_str());
      u8((base + "bendPositive").c_str());
      if (!ok()) return;
    }
    sk.unsupported.transformConstraints = count("transform");
    if (!ok()) return;
    for (std::size_t i = 0; i < sk.unsupported.transformConstraints; ++i) {
      const std::string base = "transform/" + std::to_string(i) + "/";
      readStringRequired((base + "name").c_str());
      if (!ok()) return;
      varint((base + "boneIndex").c_str());
      varint((base + "target").c_str());
      if (!ok()) return;
      f32((base + "rotateMix").c_str());
      f32((base + "translateMix").c_str());
      f32((base + "scaleMix").c_str());
      f32((base + "shearMix").c_str());
      f32((base + "offsetRotation").c_str());
      f32((base + "offsetX").c_str());
      f32((base + "offsetY").c_str());
      f32((base + "offsetScaleX").c_str());
      f32((base + "offsetScaleY").c_str());
      f32((base + "offsetShearY").c_str());
      if (!ok()) return;
    }
  }

  void readSlots(SourceSkeleton& sk) {
    const std::size_t n = count("slots");
    if (!ok()) return;
    sk.slots.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
      const std::string base = "slots/" + std::to_string(i) + "/";
      SourceSlot& slot = sk.slots[i];
      slot.name = readStringRequired((base + "name").c_str());
      if (!ok()) return;
      slot.boneIndex = static_cast<std::int32_t>(varint((base + "boneIndex").c_str()));
      if (!ok()) return;
      slot.colorRgba = static_cast<std::uint32_t>(i32((base + "color").c_str()));
      if (!ok()) return;
      readString(slot.attachmentName, (base + "attachment").c_str());
      if (!ok()) return;
      slot.blend = static_cast<int>(u8((base + "blend").c_str()));
      if (!ok()) return;
      if (slot.blend < 0 || slot.blend > 3) {
        fail(ReadCode::kUnknownBlendMode, "混合模式取值越界", (base + "blend").c_str(), pos_ - 1);
        return;
      }
      if (slot.boneIndex < 0 ||
          static_cast<std::size_t>(slot.boneIndex) >= sk.bones.size()) {
        fail(ReadCode::kIndexOutOfRange, "插槽绑定的骨骼索引越界", (base + "boneIndex").c_str(),
             pos_);
        return;
      }
    }
  }

  void readSkins(SourceSkeleton& sk) {
    // default 皮肤：没有名字字段
    SourceSkin def;
    readSkinBody(def, "skins/0");
    if (!ok()) return;
    sk.skins.push_back(std::move(def));

    const std::size_t extra = count("skins/extra");
    if (!ok()) return;
    sk.skins.resize(1 + extra);
    for (std::size_t i = 0; i < extra; ++i) {
      const std::string base = "skins/" + std::to_string(i + 1) + "/";
      sk.skins[i + 1].name = readStringRequired((base + "name").c_str());
      if (!ok()) return;
      readSkinBody(sk.skins[i + 1], base.c_str());
      if (!ok()) return;
    }
  }

  void readSkinBody(SourceSkin& skin, const char* base) {
    const std::size_t slotCount = count((std::string(base) + "slotCount").c_str());
    if (!ok()) return;
    skin.slots.resize(slotCount);
    for (std::size_t i = 0; i < slotCount; ++i) {
      const std::string sbase = std::string(base) + "slots/" + std::to_string(i) + "/";
      skin.slots[i].slotIndex =
          static_cast<std::int32_t>(varint((sbase + "slotIndex").c_str()));
      if (!ok()) return;
      const std::size_t attCount = count((sbase + "attCount").c_str());
      if (!ok()) return;
      skin.slots[i].attachments.resize(attCount);
      for (std::size_t j = 0; j < attCount; ++j) {
        readAttachment(skin.slots[i].attachments[j],
                       (sbase + "attachments/" + std::to_string(j)).c_str());
        if (!ok()) return;
      }
    }
  }

  /// mesh 族附件的字节遍历（type 1..5）：**已验证**，全语料 1113/1113 可精确到达 EOF。
  ///
  /// 布局与逐字节证据见 `docs/skeleton/task4_B4.md` §7。与官方 3.x runtime 的两处
  /// 关键差异（照抄必然错位，曾造成 630 例 STRING_TOO_LONG）：
  ///   · 3.2 的类型映射顺序不同：3 = weightedmesh、4 = linkedmesh（官方 3.5+ 里 3 是 linkedmesh）；
  ///   · 加权顶点的 boneCount / boneIndex 是 **float 不是 varint**（官方是先读 weighted
  ///     布尔位再用 varint 读骨数）。
  /// 顶点之后那个尾部 varint 的**语义仍未确认**，只作「尾部计数」跳过，不命名为 hullCount。
  ///
  /// 【B4 边界】这里只**遍历并计数**，不把 UV / 三角 / 顶点 / 权重写进 IR ——
  /// 保存全部源语义属后续工作包。本段能通过 ≠ 这些数据已可转换。

  /// uint16 大端数组：先一个 varint 计数，随后 count × 2 字节。
  void readShortArray(const char* base) {
    const std::size_t n = count((std::string(base) + "/count").c_str());
    if (!ok()) return;
    if (!need(n * 2, base)) return;
    pos_ += n * 2;
  }

  /// 顶点表。
  ///
  ///   非加权：(x, y) × vertexCount。
  ///   加权：每顶点 boneCount + boneCount × (boneIndex, x, y, weight)，
  ///       且 **boneCount 与 boneIndex 都是 float**，不是 varint。
  ///
  /// 这一点与官方 3.x 的 readVertices（先读一个 weighted 布尔位，再用 varint 读骨数）
  /// 不同，是 sdorica 3.2 的写法。曾按官方写法实现，全语料在加权网格上必然错位
  ///（顶点区首字节 0x40 会被当成 boneCount=64，超过骨架骨骼数）→ 已被语料证伪。
  void readVertices(std::size_t vertexCount, bool weighted, const char* base) {
    if (!weighted) {
      for (std::size_t i = 0; i < vertexCount; ++i) {
        f32(base);
        f32(base);
        if (!ok()) return;
      }
      return;
    }
    for (std::size_t i = 0; i < vertexCount; ++i) {
      const float bc = static_cast<float>(f32(base));
      if (!ok()) return;
      // 骨数必须是有限、非负、且不超上限的小整数；否则说明字节已经错位。
      if (!std::isfinite(bc) || bc < 0.f || bc > kMaxBonesPerVertex ||
          std::fabs(bc - std::floor(bc)) > 1e-4f) {
        fail(ReadCode::kCountLimit, "加权网格的 boneCount 不是合法的小整数（字节已错位）", base,
             pos_ - 4);
        return;
      }
      const auto n = static_cast<std::size_t>(bc);
      for (std::size_t j = 0; j < n; ++j) {
        f32(base);  // boneIndex（float）
        f32(base);  // x
        f32(base);  // y
        f32(base);  // weight
        if (!ok()) return;
      }
    }
  }

  /// mesh 族附件的字节遍历。
  void skipMeshAttachment(SourceAttachment& att, const char* base) {
    using A = AttachmentTypeByte;
    const std::uint8_t t = att.typeByte;
    switch (t) {
      case static_cast<std::uint8_t>(A::kMesh): ++unsupported_.mesh; break;
      case static_cast<std::uint8_t>(A::kLinkedMesh): ++unsupported_.linkedMesh; break;
      case static_cast<std::uint8_t>(A::kWeightedMesh): ++unsupported_.weightedMesh; break;
      case static_cast<std::uint8_t>(A::kWeightedLinkedMesh):
        ++unsupported_.weightedLinkedMesh;
        break;
      case static_cast<std::uint8_t>(A::kBoundingBox): ++unsupported_.boundingBox; break;
      default: ++unsupported_.otherAttachmentType; break;
    }
    const std::string b(base);
    std::optional<std::string> dummy;

    // boundingbox：vertexCount + 顶点（非加权）+ [color（仅 nonessential）]
    if (t == static_cast<std::uint8_t>(A::kBoundingBox)) {
      const std::size_t n = count((b + "/vertexCount").c_str());
      if (!ok()) return;
      readVertices(n, false, (b + "/vertices").c_str());
      if (!ok()) return;
      if (nonessential_) i32((b + "/color").c_str());
      return;
    }

    readString(dummy, (b + "/path").c_str());
    if (!ok()) return;
    i32((b + "/color").c_str());
    if (!ok()) return;

    // linkedmesh / weightedlinkedmesh：只引用父网格，没有自己的顶点数据
    if (t == static_cast<std::uint8_t>(A::kLinkedMesh) ||
        t == static_cast<std::uint8_t>(A::kWeightedLinkedMesh)) {
      readString(dummy, (b + "/skinName").c_str());
      if (!ok()) return;
      readString(dummy, (b + "/parent").c_str());
      if (!ok()) return;
      u8((b + "/inheritDeform").c_str());
      if (!ok()) return;
      if (nonessential_) {
        f32((b + "/width").c_str());
        f32((b + "/height").c_str());
      }
      return;
    }

    // mesh / weightedmesh：vertexCount → UV(2n) → 三角(u16 数组) → 顶点 → hull
    const std::size_t n = count((b + "/vertexCount").c_str());
    if (!ok()) return;
    for (std::size_t i = 0; i < n; ++i) {
      f32((b + "/uvs").c_str());
      f32((b + "/uvs").c_str());
      if (!ok()) return;
    }
    readShortArray((b + "/triangles").c_str());
    if (!ok()) return;
    readVertices(n, t == static_cast<std::uint8_t>(A::kWeightedMesh), (b + "/vertices").c_str());
    if (!ok()) return;
    count((b + "/hull").c_str());
    if (!ok()) return;
    // 非必要字段：edges(u16 数组) + width + height
    if (nonessential_) {
      readShortArray((b + "/edges").c_str());
      if (!ok()) return;
      f32((b + "/width").c_str());
      f32((b + "/height").c_str());
    }
  }

  void readAttachment(SourceAttachment& att, const char* base) {
    att.key = readStringRequired((std::string(base) + "/key").c_str());
    if (!ok()) return;
    readString(att.name, (std::string(base) + "/name").c_str());
    if (!ok()) return;
    att.typeByte = u8((std::string(base) + "/type").c_str());
    if (!ok()) return;
    if (att.typeByte >= 1 && att.typeByte <= 5) {
      skipMeshAttachment(att, base);
      if (!ok()) return;
      // 本任务不把 mesh 映射到 canonical：只留键与类型供 B5 报告，不生成伪空 region
      att.name.reset();
      att.region = SourceRegionAttachment{};
      return;
    }
    // 1..5 已在 skipMeshAttachment 里按已验证布局遍历；6 及以上没有独立证据，
    // 明确拒绝而不猜测字段长度（规范 §1「不得猜测」）。
    if (att.typeByte != static_cast<std::uint8_t>(AttachmentTypeByte::kRegion)) {
      fail(ReadCode::kUnknownAttachmentType,
           "附件类型字节的布局尚未经独立字节走查验证，拒绝而非猜测",
           std::string(base) + "/type", pos_ - 1);
      return;
    }
    SourceRegionAttachment& r = att.region;
    r.name = att.name.value_or(att.key);
      readString(r.path, (std::string(base) + "/path").c_str());
      if (!ok()) return;
      r.rotationDeg = f32((std::string(base) + "/rotationDeg").c_str());
      r.x = f32((std::string(base) + "/x").c_str());
      r.y = f32((std::string(base) + "/y").c_str());
      r.scaleX = f32((std::string(base) + "/scaleX").c_str());
      r.scaleY = f32((std::string(base) + "/scaleY").c_str());
      r.width = f32((std::string(base) + "/width").c_str());
      r.height = f32((std::string(base) + "/height").c_str());
      if (!ok()) return;
      r.colorRgba = static_cast<std::uint32_t>(i32((std::string(base) + "/color").c_str()));
  }

  void readEvents(SourceSkeleton& sk) {
    sk.unsupported.events = count("events");
    if (!ok()) return;
    for (std::size_t i = 0; i < sk.unsupported.events; ++i) {
      const std::string base = "events/" + std::to_string(i) + "/";
      readStringRequired((base + "name").c_str());
      if (!ok()) return;
      varint((base + "int").c_str());
      f32((base + "float").c_str());
      std::optional<std::string> dummy;
      readString(dummy, (base + "string").c_str());
      if (!ok()) return;
    }
  }

  void readAnimations(SourceSkeleton& sk) {
    sk.unsupported.animations = count("animations");
    if (!ok()) return;
    for (std::size_t a = 0; a < sk.unsupported.animations; ++a) {
      const std::string base = "animations/" + std::to_string(a) + "/";
      readStringRequired((base + "name").c_str());
      if (!ok()) return;
      readTimelineGroups(base + "slots/", sk);
      if (!ok()) return;
      readTimelineGroups(base + "bones/", sk);
      if (!ok()) return;
      readIkTimelines(base + "ik/", sk);
      if (!ok()) return;
      readTransformTimelines(base + "transform/", sk);
      if (!ok()) return;
      readDeformTimelines(base + "deform/", sk);
      if (!ok()) return;
      readDrawOrderTimelines(base + "drawOrder/", sk);
      if (!ok()) return;
      readEventTimelines(base + "event/", sk);
      if (!ok()) return;
    }
  }

  /// slot / bone / ik / transform 四组结构相同：count → (index, timelineCount) → timelines
  void readTimelineGroups(const std::string& base, SourceSkeleton& sk) {
    const std::size_t n = count(base.c_str());
    if (!ok()) return;
    for (std::size_t i = 0; i < n; ++i) {
      const std::string g = base + std::to_string(i) + "/";
      varint((g + "index").c_str());
      if (!ok()) return;
      const std::size_t tlCount = count((g + "timelineCount").c_str());
      if (!ok()) return;
      for (std::size_t t = 0; t < tlCount; ++t) {
        readTimeline((g + "timelines/" + std::to_string(t)).c_str(), sk);
        if (!ok()) return;
      }
    }
  }

  /// ik 段：count → 逐约束(index, frameCount) → 逐帧(time, mix, bendPositive, curve)。
  ///
  /// **与 slot / bone 段形状不同**：这里没有「timelineCount」也没有「类型字节」两层，
  /// 约束下标之后直接就是帧数。曾按统一形状读，全语料在 ik 段炸出 500+ 例
  /// UNKNOWN_TIMELINE_TYPE / COUNT_LIMIT（把帧的第一个浮点当成了类型字节）。
  void readIkTimelines(const std::string& base, SourceSkeleton& sk) {
    const std::size_t n = count(base.c_str());
    if (!ok()) return;
    for (std::size_t i = 0; i < n; ++i) {
      const std::string g = base + std::to_string(i) + "/";
      varint((g + "index").c_str());
      if (!ok()) return;
      ++sk.unsupported.timelines;
      const std::size_t frameCount = count((g + "frameCount").c_str());
      if (!ok()) return;
      for (std::size_t f = 0; f < frameCount; ++f) {
        const std::string fb = g + "frames/" + std::to_string(f);
        f32((fb + "/time").c_str());
        if (!ok()) return;
        f32((fb + "/mix").c_str());
        if (!ok()) return;
        u8((fb + "/bendPositive").c_str());
        if (!ok()) return;
        if (f + 1 < frameCount) {
          readCurve((fb + "/curve").c_str());
          if (!ok()) return;
        }
      }
    }
  }

  /// transform 段：count → 逐约束(index, frameCount) → 逐帧(time, 四个 mix, curve)。
  /// 同样没有 timelineCount / 类型字节两层。
  void readTransformTimelines(const std::string& base, SourceSkeleton& sk) {
    const std::size_t n = count(base.c_str());
    if (!ok()) return;
    for (std::size_t i = 0; i < n; ++i) {
      const std::string g = base + std::to_string(i) + "/";
      varint((g + "index").c_str());
      if (!ok()) return;
      ++sk.unsupported.timelines;
      const std::size_t frameCount = count((g + "frameCount").c_str());
      if (!ok()) return;
      for (std::size_t f = 0; f < frameCount; ++f) {
        const std::string fb = g + "frames/" + std::to_string(f);
        f32((fb + "/time").c_str());
        if (!ok()) return;
        f32((fb + "/rotateMix").c_str());
        f32((fb + "/translateMix").c_str());
        f32((fb + "/scaleMix").c_str());
        f32((fb + "/shearMix").c_str());
        if (!ok()) return;
        if (f + 1 < frameCount) {
          readCurve((fb + "/curve").c_str());
          if (!ok()) return;
        }
      }
    }
  }

  void readTimeline(const char* base, SourceSkeleton& sk) {
    const std::uint8_t kind = u8((std::string(base) + "/type").c_str());
    if (!ok()) return;
    ++sk.unsupported.timelines;
    const std::size_t frameCount = count((std::string(base) + "/frameCount").c_str());
    if (!ok()) return;
    for (std::size_t f = 0; f < frameCount; ++f) {
      const std::string fb = std::string(base) + "/frames/" + std::to_string(f);
      f32((fb + "/time").c_str());  // 所有时间线首值都是 time
      if (!ok()) return;
      switch (kind) {
        case kTlRotate:
          f32((fb + "/value").c_str());
          break;
        case kTlTranslate:
        case kTlScale:
        case kTlShear:
          f32((fb + "/x").c_str());
          f32((fb + "/y").c_str());
          break;
        case kTlAttachment: {
          std::optional<std::string> dummy;
          readString(dummy, (fb + "/attachment").c_str());
          break;
        }
        case kTlColor:
          i32((fb + "/color").c_str());
          break;
        case kTlDeform: {
          const std::size_t vc = count((fb + "/vertexCount").c_str());
          if (!ok()) return;
          for (std::size_t k = 0; k < vc; ++k) f32((fb + "/vertices").c_str());
          break;
        }
        case kTlIk:
          f32((fb + "/mix").c_str());
          u8((fb + "/bendPositive").c_str());
          break;
        case kTlTransform:
          f32((fb + "/rotateMix").c_str());
          f32((fb + "/translateMix").c_str());
          f32((fb + "/scaleMix").c_str());
          f32((fb + "/shearMix").c_str());
          break;
        case kTlDrawOrder:
        case kTlEvent:
          // 出现在独立数组里，不走这里
          fail(ReadCode::kUnknownTimelineType,
               "drawOrder / event 时间线不应出现在按目标分组的段里", std::string(base) + "/type",
               pos_);
          return;
        default:
          fail(ReadCode::kUnknownTimelineType, "未知时间线类型字节",
               std::string(base) + "/type", pos_);
          return;
      }
      if (!ok()) return;
      if (timelineHasCurve(kind) && f + 1 < frameCount) {
        readCurve((fb + "/curve").c_str());
        if (!ok()) return;
      }
    }
  }

  void readCurve(const char* base) {
    const std::uint8_t type = u8(base);
    if (!ok()) return;
    if (type == 2) {
      f32((std::string(base) + "/cx1").c_str());
      f32((std::string(base) + "/cy1").c_str());
      f32((std::string(base) + "/cx2").c_str());
      f32((std::string(base) + "/cy2").c_str());
      return;
    }
    if (type == 0 || type == 1) return;  // linear / stepped
    fail(ReadCode::kUnknownTimelineType, "未知曲线类型字节", base, pos_ - 1);
  }

  /// deform 段：skinCount → 逐 skin(skinIndex, slotCount) → 逐 slot(slotIndex,
  /// attachmentCount) → 逐附件(attachmentName, frameCount) → 逐帧(time,
  /// vertexCount, vertices, curve)
  ///
  /// 由 b0015s5_meat 尾段穷举**唯一命中精确 EOF** 的组合确定（见 B4 报告 §8）：
  /// 原先按「skinName(string) + attachmentName + timelineCount」读会错位 ——
  /// frameCount 会读到 0x6d=109，顶点数组随即越界。按皮肤名读时 "meat1"
  /// 被当成 skinName，而它实际是**附件名**。
  /// 帧含 curve（非末帧），顶点数可为 0。
  void readDeformTimelines(const std::string& base, SourceSkeleton& sk) {
    const std::size_t skinCount = count(base.c_str());
    if (!ok()) return;
    for (std::size_t i = 0; i < skinCount; ++i) {
      const std::string sb = base + std::to_string(i) + "/";
      varint((sb + "skinIndex").c_str());
      if (!ok()) return;
      const std::size_t slotCount = count((sb + "slotCount").c_str());
      if (!ok()) return;
      for (std::size_t j = 0; j < slotCount; ++j) {
        const std::string ssb = sb + "slots/" + std::to_string(j) + "/";
        varint((ssb + "slotIndex").c_str());
        if (!ok()) return;
        const std::size_t attCount = count((ssb + "attachmentCount").c_str());
        if (!ok()) return;
        for (std::size_t a = 0; a < attCount; ++a) {
          std::optional<std::string> dummy;
          readString(dummy, (ssb + std::to_string(a) + "/attachmentName").c_str());
          if (!ok()) return;
          ++sk.unsupported.timelines;
          ++sk.unsupported.deformTimelines;
          const std::string tb = ssb + std::to_string(a) + "/timeline";
          const std::size_t frameCount = count((tb + "/frameCount").c_str());
          if (!ok()) return;
          for (std::size_t f = 0; f < frameCount; ++f) {
            const std::string fb = tb + "/frames/" + std::to_string(f);
            f32((fb + "/time").c_str());
            if (!ok()) return;
            // 顶点区：endVertices，且**仅当其非零**才跟一个 startVertices，
            // 浮点个数 = end - start。直接把 end 当浮点个数会少读一个 varint，
            // 使后续 curve 错位（umbrella_fly：end=22、start=0 → 22 个浮点）。
            const std::size_t end = count((fb + "/endVertices").c_str());
            if (!ok()) return;
            std::size_t n = 0;
            if (end != 0) {
              // startVertices 照读（顶点起始下标），浮点个数按 endVertices 计：
              // 减法版本在 m0020c2 等样本上会得到负数（end=1, start=10），不成立。
              const std::size_t start = count((fb + "/startVertices").c_str());
              if (!ok()) return;
              (void)start;
              n = end;
            }
            for (std::size_t k = 0; k < n; ++k) f32((fb + "/vertices").c_str());
            if (!ok()) return;
            if (f + 1 < frameCount) {
              readCurve((fb + "/curve").c_str());
              if (!ok()) return;
            }
          }
        }
      }
    }
  }

  void readDrawOrderTimelines(const std::string& base, SourceSkeleton& sk) {
    const std::size_t n = count(base.c_str());
    if (!ok()) return;
    for (std::size_t i = 0; i < n; ++i) {
      ++sk.unsupported.timelines;
      const std::string tb = base + std::to_string(i) + "/";
      f32((tb + "time").c_str());
      if (!ok()) return;
      const std::size_t offsetCount = count((tb + "offsetCount").c_str());
      if (!ok()) return;
      for (std::size_t j = 0; j < offsetCount; ++j) {
        varint((tb + "offsets").c_str());
        varint((tb + "offsets").c_str());
        if (!ok()) return;
      }
    }
  }

  void readEventTimelines(const std::string& base, SourceSkeleton& sk) {
    const std::size_t n = count(base.c_str());
    if (!ok()) return;
    for (std::size_t i = 0; i < n; ++i) {
      ++sk.unsupported.timelines;
      const std::string tb = base + std::to_string(i) + "/";
      f32((tb + "time").c_str());
      if (!ok()) return;
      std::optional<std::string> dummy;
      readString(dummy, (tb + "name").c_str());
      if (!ok()) return;
      varint((tb + "int").c_str());
      f32((tb + "float").c_str());
      readString(dummy, (tb + "string").c_str());
      if (!ok()) return;
    }
  }
};

}  // namespace

const char* toString(ReadCode code) noexcept {
  switch (code) {
    case ReadCode::kOk: return "OK";
    case ReadCode::kTruncated: return "TRUNCATED";
    case ReadCode::kTrailingBytes: return "TRAILING_BYTES";
    case ReadCode::kVarintOverflow: return "VARINT_OVERFLOW";
    case ReadCode::kStringTooLong: return "STRING_TOO_LONG";
    case ReadCode::kCountLimit: return "COUNT_LIMIT";
    case ReadCode::kUnsupportedVersion: return "UNSUPPORTED_VERSION";
    case ReadCode::kNonFiniteFloat: return "NON_FINITE_FLOAT";
    case ReadCode::kUnknownAttachmentType: return "UNKNOWN_ATTACHMENT_TYPE";
    case ReadCode::kUnknownTimelineType: return "UNKNOWN_TIMELINE_TYPE";
    case ReadCode::kUnknownBlendMode: return "UNKNOWN_BLEND_MODE";
    case ReadCode::kIndexOutOfRange: return "INDEX_OUT_OF_RANGE";
    case ReadCode::kInvalidUtf8: return "INVALID_UTF8";
  }
  return "UNKNOWN";
}

ReadResult readSkeletonBinary32(const std::vector<std::uint8_t>& bytes) {
  ReadResult result;
  SourceSkeleton sk;
  Reader reader(bytes, result.diagnostics, sk.unsupported);
  const bool ok = reader.run(sk);
  result.ok = ok;
  if (ok) {
    result.skeleton = std::move(sk);
  } else {
    // 失败不留半成品 IR，但仍把已统计到的不支持能力交回，供报告使用。
    result.skeleton.unsupported = std::move(sk.unsupported);
  }
  return result;
}

}  // namespace loongbones::importers::spine32
