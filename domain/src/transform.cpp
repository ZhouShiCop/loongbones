// Transform 的有限性判定（校验规则 10 的唯一判据）。

#include "loongbones/domain/transform.hpp"

#include <cmath>

namespace loongbones {

bool Transform::isFinite() const noexcept {
  return std::isfinite(x) && std::isfinite(y) && std::isfinite(rotationDeg) &&
         std::isfinite(scaleX) && std::isfinite(scaleY);
}

}  // namespace loongbones