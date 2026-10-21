// 图集资源领域模型（Task 2 4.3）
// 纯数据: 不解析 Spine .atlas、不解码图片、不计算 quad/UV。
// 身份与实例分离: 一个 Region 可被多个图片节点引用, 节点只存稳定 ID。
// 单位: 所有尺寸与偏移统一为像素; 旋转用显式枚举。
// 变换语义沿用 ADR-0003, 本文件不另造坐标约定。

#ifndef LOONGBONES_DOMAIN_ATLAS_HPP
#define LOONGBONES_DOMAIN_ATLAS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "loongbones/domain/ids.hpp"
#include "loongbones/domain/image_asset.hpp"

namespace loongbones {

using TextureAssetId = Id;
using AtlasId = Id;
using AtlasPageId = Id;
using AtlasRegionId = Id;

/// 各自种类的 ID 构造器，保证强类型区分（不会把 NodeId 当成 RegionId 用）
inline TextureAssetId newTextureAssetId() { return Id::generate(IdKind::TextureAsset); }
inline AtlasId newAtlasId() { return Id::generate(IdKind::Atlas); }
inline AtlasPageId newAtlasPageId() { return Id::generate(IdKind::AtlasPage); }
inline AtlasRegionId newAtlasRegionId() { return Id::generate(IdKind::AtlasRegion); }
/// 区域在页内的旋转状态。显式枚举, 不用布尔（布尔无法表达 180/270）。
enum class RegionRotation {
  kNone = 0,
  kDeg90 = 90,
  kDeg180 = 180,
  kDeg270 = 270,
};

inline bool isValidRotation(RegionRotation r) noexcept {
  switch (r) {
    case RegionRotation::kNone:
    case RegionRotation::kDeg90:
    case RegionRotation::kDeg180:
    case RegionRotation::kDeg270:
      return true;
  }
  return false;
}

/// 90/270 时图集内实际占位是转置的
inline bool rotationSwapsExtent(RegionRotation r) noexcept {
  return r == RegionRotation::kDeg90 || r == RegionRotation::kDeg270;
}
/// 纹理资产：一张图片文件（图集页的像素来源）。
struct TextureAsset {
  TextureAssetId id;
  std::string name;
  StorageMode storage = StorageMode::kExternalReference;
  /// 位置标识。本 Task 不解析、不校验存在性。
  std::string location;
  /// 可选像素尺寸；未装载前可缺省
  std::optional<std::int32_t> pixelWidth;
  std::optional<std::int32_t> pixelHeight;
};

/// 图集页：引用一张纹理资产，承载若干区域。
struct AtlasPage {
  AtlasPageId id;
  std::string name;
  TextureAssetId textureAssetId;
  std::int32_t width = 0;
  std::int32_t height = 0;
};
/// 图集区域：资源记录，不是节点。所有尺寸/偏移单位为像素。
///   packedX/Y/W/H  页内打包矩形（左上角 + 尺寸）
///   rotation       打包旋转; 仅影响页内实际占位（90/270 时转置）
///   originalW/H    未裁剪未打包的原始图像尺寸
///   trimmedW/H     裁切后尺寸; 方向与 original 一致（不受打包旋转影响）; 无裁切时 == original
///   offsetX/Y      trim 偏移; 方向与 original 一致（不受打包旋转影响）; 无裁切时默认 0
struct AtlasRegion {
  AtlasRegionId id;
  std::string name;
  AtlasPageId pageId;
  std::int32_t packedX = 0;
  std::int32_t packedY = 0;
  std::int32_t packedW = 0;
  std::int32_t packedH = 0;
  RegionRotation rotation = RegionRotation::kNone;
  std::int32_t originalW = 0;
  std::int32_t originalH = 0;
  std::int32_t trimmedW = 0;
  std::int32_t trimmedH = 0;
  std::int32_t offsetX = 0;
  std::int32_t offsetY = 0;

  bool hasNoTrim() const noexcept {
    return trimmedW == originalW && trimmedH == originalH && offsetX == 0 && offsetY == 0;
  }
  bool hasValidRect() const noexcept {
    return packedX >= 0 && packedY >= 0 && packedW > 0 && packedH > 0;
  }
  bool hasValidSizes() const noexcept {
    return originalW > 0 && originalH > 0 && trimmedW > 0 && trimmedH > 0;
  }
};
/// 图集: 一组页与区域的集合。
struct Atlas {
  AtlasId id;
  std::string name;
  std::vector<AtlasPage> pages;
  std::vector<AtlasRegion> regions;
};

}  // namespace loongbones

#endif  // LOONGBONES_DOMAIN_ATLAS_HPP