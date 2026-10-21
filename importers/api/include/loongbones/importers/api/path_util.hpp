// Task 4 B6: 导入用路径工具（授权根相对、词法规范化、包含判定）。
//
// 为什么放在 `api/` 而不是 `host/`：插件与宿主都要用同一套判定，才能保证
// 「插件给什么路径、宿主就按什么口径拒」。放 `host/` 会造成 插件 → 宿主 的反向依赖。
//
// 这些函数是**纯函数**，不碰文件系统（`isPathWithin` 只比较已给出的路径），
// 因此可以被测试直接断言，也可以被宿主在调用 `std::filesystem::canonical()` 之后复用。

#ifndef LOONGBONES_IMPORTERS_API_PATH_UTIL_HPP
#define LOONGBONES_IMPORTERS_API_PATH_UTIL_HPP

#include <filesystem>
#include <string>
#include <string_view>

namespace loongbones::importers::api {

/// 是否可作为「授权根相对路径」被接受：非空、相对、无盘符/无根、无反斜杠、
/// 无空段、无 `.`、无 `..`、无 NUL。
[[nodiscard]] bool isSafeRelativePath(std::string_view relative) noexcept;

/// 词法规范化：去掉 `.` 与多余 `/`，解析 `..`（越出根部则整段退回失败）。
/// 返回空串表示无法安全规范化（例如 `../x`）。
[[nodiscard]] std::string normalizedRelativePath(std::string_view relative);

/// `candidate` 是否位于 `root` 之内（含相等）。调用方负责先把 `candidate`
/// 过一遍 `std::filesystem::canonical()` —— 否则符号链接逃逸不会命中。
[[nodiscard]] bool isPathWithin(const std::filesystem::path& root,
                                const std::filesystem::path& candidate);

}  // namespace loongbones::importers::api

#endif  // LOONGBONES_IMPORTERS_API_PATH_UTIL_HPP
