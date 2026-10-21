// Spine 3.2.01 二进制 Reader（Task 4 B4：只解析，不映射）
//
// 边界（Task4_Implementation_Plan.md B4）：
//   · 输出私有的 SourceSkeleton IR（src/source_model.hpp），不做源 → canonical 映射（属 B5）；
//   · 每次读取都带 offset 与段名，诊断可定位到字节；
//   · 读取前检查剩余长度、长度上限与整数溢出；float32 立即 isfinite；
//   · 文件必须 strict EOF（尾随字节 = 失败）；
//   · 版本只接受头版本 3.2.01；未知附件类型明确拒绝，不猜测。
//
// 不链接、不复制 Spine Runtime。布局依据是对语料的独立字节走查
// （docs/skeleton/field-mapping.md + tools/skeleton-probe/layout-explore.mjs）。

#ifndef LOONGBONES_IMPORTERS_SPINE32_BINARY_READER_HPP
#define LOONGBONES_IMPORTERS_SPINE32_BINARY_READER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "loongbones/importers/spine32/source_model.hpp"

namespace loongbones::importers::spine32 {

/// Reader 结果码（稳定；程序逻辑按枚举判断，不按文案）。
enum class ReadCode {
  kOk = 0,
  kTruncated,           ///< 剩余字节不足
  kTrailingBytes,       ///< 解析完仍有尾随字节（strict EOF 失败）
  kVarintOverflow,      ///< varint 超过 32 位或字节数超限
  kStringTooLong,       ///< 长度前缀字符串超过上限
  kCountLimit,          ///< 数组长度超过上限
  kUnsupportedVersion,  ///< 头版本不是 3.2.01
  kNonFiniteFloat,      ///< float32 为 NaN / ±Inf（field-mapping §12）
  kUnknownAttachmentType,
  kUnknownTimelineType,
  kUnknownBlendMode,
  kIndexOutOfRange,  ///< 源索引越界（结构闭合校验）
  kInvalidUtf8,
};

const char* toString(ReadCode code) noexcept;

struct ReadDiagnostic {
  ReadCode code = ReadCode::kOk;
  /// 面向人的说明；不得用于程序分支。
  std::string message;
  /// 段名，例如 "bones/1/rotationDeg"、"skins/0/entries/2/attachment"
  std::string section;
  /// 出错处的字节偏移
  std::size_t offset = 0;
};

struct ReadResult {
  bool ok = false;
  std::vector<ReadDiagnostic> diagnostics;
  SourceSkeleton skeleton;
};

/// 解析一份 `.skel.bytes`。只读入参，不碰文件系统。
/// 任何结构性错误都会使 ok == false 且 skeleton 为空；不做部分成功。
ReadResult readSkeletonBinary32(const std::vector<std::uint8_t>& bytes);

/// 诊断里的上限常量（暴露以便测试引用）。
inline constexpr std::size_t kMaxVarintBytes = 5;    ///< 32 位 varint 最多 5 字节
inline constexpr std::size_t kMaxStringBytes = 4096;  ///< 单个字符串上限
inline constexpr std::size_t kMaxArrayCount = 1u << 20;  ///< 数组长度上限

}  // namespace loongbones::importers::spine32

#endif  // LOONGBONES_IMPORTERS_SPINE32_BINARY_READER_HPP
