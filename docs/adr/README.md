# ADR / RFC 草案（Phase 0 产出，待负责人签定）

对应规范：`项目规范.md` §8.5（`docs/adr/NNNN-短标题.md`）、§12 交付物 7。

**这些是草案，不是决定。** 每篇都列出：待决问题、可选方案、已知约束、
建议的验证方式。签定后由负责人在文件头改 `状态：已签定` 并记录日期与理由。

| 编号 | 标题 | 阻塞什么 |
|---|---|---|
| [0001](0001-tech-stack-qt-and-render-backend.md) | 技术栈：语言 / Qt 绑定 / 构建系统 / 渲染后端 / 目标平台 | Phase 1 垂直切片 |
| [0002](0002-native-format-container.md) | 原生格式容器方案（JSON / 二进制 / 压缩 / 增量） | Schema RFC |
| [0003](0003-units-and-coordinate-contract.md) | 坐标 / 角度 / 时基 / 曲线 / UV 原点的 schema 级契约 | 所有 Domain 字段 |
| [0004](0004-stable-identity-strategy.md) | 稳定 ID 策略与索引迁移 | 对象引用、权重绑定 |

## 为什么先定 0003

旧项目（Spine↔DragonBones 转换器）的教训：绝大多数"渲染散开/UV 翻转"类缺陷
最终都追溯到**语义歧义**而非算法错误——坐标系方向、非加权 mesh 顶点空间、
deform 增量域、骨骼帧值是绝对值还是增量、rotate 的布尔与度数表示。
这些若不写成 schema 级书面契约，每个消费端都会各自解释一遍（规范 §3.2 已明确要求）。
因此 0003 建议在写任何 Domain 类型之前签定。
