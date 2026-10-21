// 图片资源元数据（Task1 §3.5）
//
// 表示资源的逻辑身份，不是组合中的实例。本任务不解析路径、不读文件、
// 不校验文件存在（§3.5）；路径安全由后续 Asset 模块负责（规范 §8.2）。

#ifndef LOONGBONES_DOMAIN_IMAGE_ASSET_HPP
#define LOONGBONES_DOMAIN_IMAGE_ASSET_HPP

#include <cstdint>
#include <optional>
#include <string>

#include "loongbones/domain/ids.hpp"

namespace loongbones {

/// 图片资源类型。首版只有静态图片；序列帧属后续阶段（DECISIONS.md D10）。
enum class ImageAssetType {
  kStatic,
};

/// 存储模式（§3.5）
enum class StorageMode {
  kExternalReference,  ///< 引用工程外部的图片文件
  kInProject,          ///< 图片存放在工程内
};

/// 图片资源元数据。
///
/// 权威性：ImageNode 只引用本对象的 ID，不复制这里的元数据（§3.4）；
/// 同一资源可被多个 ImageNode 引用（§3.4）。
struct ImageAsset {
  ImageAssetId id;
  std::string name;  ///< 展示名，在本工程内唯一
  ImageAssetType type = ImageAssetType::kStatic;
  StorageMode storage = StorageMode::kExternalReference;
  /// 资源位置标识。本任务不解析它。
  std::string location;
  /// 可选的像素尺寸；未加载前可缺省
  std::optional<std::int32_t> pixelWidth;
  std::optional<std::int32_t> pixelHeight;
};

}  // namespace loongbones

#endif  // LOONGBONES_DOMAIN_IMAGE_ASSET_HPP