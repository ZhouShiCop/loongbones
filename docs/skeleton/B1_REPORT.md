# Task 4 B1 验收报告：Atlas + PNG 首个垂直切片

- 日期：2026-10-20
- 状态：实现与验证完成，待负责人复核
- 范围：Spine/libGDX Atlas 文本与其引用的 PNG；未读取 `.skel.bytes`，未创建 Skeleton/Bone/Slot/Skin/Attachment；未使用 Spine Runtime。
- 后续（2026-10-21 更新）：B2–B6 此后已分别获准并完成实现，见 `B3_REPORT.md` / `task4_B4.md` / `B5_REPORT.md` / `B6_REPORT.md`；本行保留的是 B1 交付当时的状态。

## 交付

| 路径 | 内容 |
|---|---|
| `importers/spine32/` | Qt 无关的 Atlas 解析、PNG IHDR 检查、canonical Atlas/Texture 候选构造、资源路径 Resolver 与暂存提交实现 |
| `tests/importers/spine32/test_atlas_import.cpp` | 8 项导入测试 |
| `tools/atlas-import-tests/` | 专项测试构建与运行说明 |
| `tools/atlas-import-smoke/` | 无 GUI 的真实文件导入入口 |
| `STRUCTURE.md` | 新模块与两个验证工具登记 |

## 行为契约

- 支持 Atlas 页/Region、多页、`rotate`、`xy`、`size`、`orig`、`offset`、`index`；页坐标尺寸与 PNG 像素尺寸分别保存，Region 坐标不按 PNG 尺寸缩放。
- 同时接受标准 Atlas 文本和语料里观察到的 Unity `TextAsset` 包装；接受页属性之后无空行直接开始首个 Region 的布局。
- 仅检查 PNG signature、IHDR、IHDR CRC 和正 int32 尺寸，不解码像素。
- 尚不进入 canonical 的 Atlas 属性（例如 `format`、`filter`、`repeat`、`pma`）发出 warning；用户未显式接受 warning 时不暂存或写入资源。
- 源文件通过只读 Resolver 读取；输入相对路径与规范化后的路径必须留在授权根内。纹理按 `assets/textures/<TextureAssetId>.png` 写入工程。
- 候选先由 Domain 全量校验；资源暂存/发布成功后保存工程；保存失败会回滚新发布纹理，接收方 `ProjectDocument` 不变。
- 本切片使用现有 native v1 Codec 保存 Atlas/Texture。v2 Skeleton Codec 属 B3；B1 未改动 v1 Schema，也未宣称 v2 Codec 已实现。

## 验证证据

| 验证 | 结果 |
|---|---|
| 全量构建 | `build_b1` 全部目标成功 |
| Core tests | CTest 通过 |
| Format tests | CTest 通过 |
| Atlas importer tests | 8/8 通过，0 失败 |
| Format probe selftest | CTest 通过 |
| 完整 CTest | 4/4 通过，退出码 0 |
| 真实语料 smoke | manifest 样本 #01：1 页、22 Regions、1 PNG；未确认 warning 时退出码 3 且不写文件；确认后退出码 0，工程与 PNG 落盘 |
| 构建告警 | 本轮构建输出未见编译告警 |

专项测试覆盖：trim/rotation 与页尺寸分离、多页、Unity TextAsset 包装及无空行页/Region 边界、路径穿越在资源访问前被拒绝、PNG IHDR CRC 与 trim 校验、warning 确认门槛、保存失败回滚、v1 保存并重开语义检查。

## 限制与后续

- B1 不提供 GUI、图像预览/解码、Skeleton/附件映射、`.skel.bytes` 读取、通用 ImportPlugin 注册表或动态库 ABI。
- IHDR 通过不等于完整 PNG 压缩数据已解码验证；像素完整性与预览属于后续图像/渲染工作。
- `format/filter/repeat/pma` 等属性目前只有诊断，不持久化；其中 `pma` 对后续绘制混合可能有语义影响，应在进入渲染前单独裁定数据模型并补测试。
- Atlas 区块和路径检查仍需在更丰富的真实语料与合成反例中扩展；本次真实 smoke 只验证一个 manifest 样本，不代表全量 1113 个输入全部兼容。
