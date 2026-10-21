// 稳定标识（ID）——契约见 Loongbones/项目规范.md §3.2、§10.1
//
// 契约（D 来自 docs/adr/0004）：
//   · 不可变：创建后任何操作不得改写
//   · 不含语义：不得由名称、数组位置、文件路径推导
//   · 重命名不影响 ID
//
// 形式：ULID（26 字符 Crockford Base32）。理由见 ADR-0004 §5.1：
//   时间前缀使 ID 字符串排序即创建顺序（对 Outliner / Asset Browser 的稳定排序有用），
//   且可零依赖自行实现，不引入第三方库（规范 §8.3 要求新增依赖前论证）。
//
// 注意：本文件的生成策略（随机 ULID）是 ADR-0004 推荐方案 C 的「第一阶段落地」：
//   当前只实现随机生成；「导入对象的确定性派生」路径属 Phase 2 导入器范围，尚未实现。
//   ADR-0004 签定后若策略变化，只需替换本文件，模型其余部分不受影响。

#ifndef LOONGBONES_DOMAIN_IDS_HPP
#define LOONGBONES_DOMAIN_IDS_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace loongbones {

/// Crockford Base32 字母表（排除 I、L、O、U，避免手抄与语音转写歧义）
inline constexpr char kCrockfordBase32[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
inline constexpr std::size_t kUlidLength = 26;

/// 标识的种类。不同种类用强类型区分，避免把 ProjectId 传成 NodeId
/// （规范 §8.3：以显式类型表达业务状态，避免裸字符串传递对象种类）。
enum class IdKind {
  Project,
  Scene,       ///< Task 2
  Node,
  ImageAsset,
  TextureAsset,  ///< Task 2
  Atlas,         ///< Task 2
  AtlasPage,     ///< Task 2
  AtlasRegion,   ///< Task 2
  Skeleton,      ///< Task 4 B2
  Bone,          ///< Task 4 B2
  Slot,          ///< Task 4 B2
  Skin,          ///< Task 4 B2
  SkinEntry,     ///< Task 4 B2
};

/// 强类型标识。值语义、可比较；不携带任何可被当作语义使用的附加信息。
class Id {
 public:
  Id() = default;

  /// 用已存在的字符串构造（导入 / 反序列化用）。
  /// 非法格式不会被静默接受：请配合 isWellFormed() 校验。
  static Id fromString(IdKind kind, std::string_view text);

  /// 生成一个新的随机 ULID。
  static Id generate(IdKind kind);

  /// 是否是「无 ID」（默认构造）。根节点无父节点时用它表示。
  [[nodiscard]] bool isNone() const noexcept { return value_.empty(); }

  [[nodiscard]] IdKind kind() const noexcept { return kind_; }
  [[nodiscard]] const std::string& value() const noexcept { return value_; }

  /// 格式是否合法：26 字符、字符合法集、首位字符编码 <=3 bit。
  /// 只校验格式，不解析时间戳——ID 对模型而言是不透明的。
  [[nodiscard]] bool isWellFormed() const noexcept;

  [[nodiscard]] std::string str() const { return value_; }

  friend bool operator==(const Id& a, const Id& b) noexcept {
    return a.kind_ == b.kind_ && a.value_ == b.value_;
  }
  friend bool operator!=(const Id& a, const Id& b) noexcept { return !(a == b); }
  /// 排序：先按种类，再按字符串（ULID 定长大写，字典序即创建顺序）
  friend bool operator<(const Id& a, const Id& b) noexcept {
    if (a.kind_ != b.kind_) return a.kind_ < b.kind_;
    return a.value_ < b.value_;
  }

 private:
  IdKind kind_ = IdKind::Node;
  std::string value_;
};

using ProjectId = Id;
using SceneId = Id;
using NodeId = Id;
using ImageAssetId = Id;

/// ULID 编解码（暴露以便测试与将来的格式探针复用）。
std::string encodeUlid(std::uint64_t unixMillis, std::uint64_t randomnessHigh,
                       std::uint64_t randomnessLow);
bool decodeUlid(std::string_view text, std::uint64_t& unixMillis,
                std::uint64_t& randomnessHigh, std::uint64_t& randomnessLow);

/// 判断字符串是否为格式合法的 ULID。
bool isWellFormedUlid(std::string_view text) noexcept;

/// 生成 80 位随机源（high 只用低 16 位）。
/// 抽出成独立函数便于测试注入固定随机源。
void generateRandom(std::uint64_t& high, std::uint64_t& low);

}  // namespace loongbones

#endif  // LOONGBONES_DOMAIN_IDS_HPP