// 二维局部变换——契约见 docs/adr/0003-units-and-coordinate-contract.md
//
// 已定案的契约（DECISIONS.md D3/D4/D5）：
//   · 坐标系 **Y-up**（不是屏幕坐标的 Y 向下）
//   · 旋转单位 **度**（字段名带 Deg）；求值时才转弧度，边界集中转换
//   · 组合顺序 **2x3 仿射、行主、world = parent x local**
//
// 本任务范围限制（Agent_Task/Task1.md §3.6）：
//   只存 x / y / rotationDeg / scaleX / scaleY 的**局部** TRS；
//   **不实现**矩阵运算、锚点、斜切、完整渲染计算。

#ifndef LOONGBONES_DOMAIN_TRANSFORM_HPP
#define LOONGBONES_DOMAIN_TRANSFORM_HPP

namespace loongbones {

/// 二维局部变换。默认值即"未改动"：位置与旋转为 0，缩放为 1。
///
/// 取值约束：数值字段必须有限；NaN / Inf 视为非法，
/// 由 Project 的写操作与整体校验拦截（Task 1 §4 校验规则 10）。
struct Transform {
  double x = 0.0;
  double y = 0.0;
  double rotationDeg = 0.0;
  double scaleX = 1.0;
  double scaleY = 1.0;

  /// 全字段是否有限（校验规则 10 的唯一判据）
  [[nodiscard]] bool isFinite() const noexcept;

  friend bool operator==(const Transform& a, const Transform& b) noexcept {
    return a.x == b.x && a.y == b.y && a.rotationDeg == b.rotationDeg &&
           a.scaleX == b.scaleX && a.scaleY == b.scaleY;
  }
  friend bool operator!=(const Transform& a, const Transform& b) noexcept { return !(a == b); }
};

/// 默认变换：位置与旋转 0、缩放 1（Task 1 §3.6）
inline constexpr Transform kDefaultTransform{};

}  // namespace loongbones

#endif  // LOONGBONES_DOMAIN_TRANSFORM_HPP