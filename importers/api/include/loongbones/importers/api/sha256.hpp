// Task 4 B6: 最小 SHA-256（FIPS 180-4），仅用于 `sourceRef.contentHash`。
//
// 为什么自己写：本工作区**不得**引入第三方加密库，也不得引入新的构建依赖
// （Task4_Implementation_Plan.md §6）。canonical 契约要求"宿主/插件对 `.skel.bytes`
// 全文计算 SHA-256"（D25），故在此提供一个无依赖、无状态、无堆分配热点的小实现。
//
// 边界：这是**校验和工具**，不是安全原语 —— 不用于口令、不用于签名、不用于抗碰撞场景。

#ifndef LOONGBONES_IMPORTERS_API_SHA256_HPP
#define LOONGBONES_IMPORTERS_API_SHA256_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace loongbones::importers::api {

/// 计算 `data[0..size)` 的 SHA-256，返回 64 个小写十六进制字符。
std::string sha256Hex(const std::uint8_t* data, std::size_t size);

/// 便利重载。
std::string sha256Hex(const std::vector<std::uint8_t>& bytes);

}  // namespace loongbones::importers::api

#endif  // LOONGBONES_IMPORTERS_API_SHA256_HPP
