# Task 5 · 阶段 A 交付报告（第二批：mesh / 蒙皮 / 约束）

- 任务：`Agent_Task/Task5.md` §3（A1…A6）
- 日期：2026-10-22
- 状态：**阶段 A 完成，停下等待负责人审阅**（A6）。**阶段 B（实现）未开工**，按 §3 停点执行
- 交付明细：A1 见 `Task5_A1_Report.md`；本文汇总 A1–A5 与阶段 A 整体

## 1. 阶段 A 交付物清单

| # | 交付物 | 位置 |
|---|---|---|
| A1 | 第二批静态数据取证（Full 1113 / Primary 963） | 报告 `Agent_Task/Task5_A1_Report.md`；证据 `docs/skeleton/mesh-constraint-evidence.md` |
| A1 | 取证探针 + 对照实验探针 | `tools/skeleton-probe/mesh-constraint-probe.mjs`、`tc1-space-control.mjs`、`type-byte-census.mjs` |
| A2 | canonical 模型提案（6 个类型） | `docs/skeleton/mesh-and-constraint-rfc.md` |
| A3 | 原生格式 v3 增量提案（D29） | `docs/format/native-project-v3-mesh.md` |
| A4 | 支持矩阵 / 诊断 / 决策同步 | `DECISIONS.md` D34–D36；`docs/skeleton/skeleton-domain-rfc.md` §10；`docs/skeleton/field-mapping.md` §11.2；`Agent_Task/DataType_Report.md`；`Task5.md` §2.1 |
| A5 | 合成 fixture **11 个**（共 16 个） | `docs/skeleton/fixtures/06…16`；预期结果 `docs/skeleton/fixtures/README.md`；复核后按负责人裁定补的 12–16（权重和 = 0 / < 0、linked 语料同构、IK `mix=0`、`nonessential=0` mesh）见 `Agent_Task/Task5_A5_Review.md` §8 |

## 2. 五项争议的最终结论（含 2026-10-22 负责人答复）

| # | 争议 | 结论 |
|---|---|---|
| 1 | 加权 mesh 顶点空间 | **补上验证**：复跑 `tc1-space-control.mjs` 逐位一致（A1 122.132 / A2 5.502 / A3 22÷204,304 / C1 34.766 / D1 435.473…）⇒ **N2 维持**（骨骼局部坐标） |
| 2 | 负权重 | **合法，意义是权重无影响** → `DECISIONS.md` **D34**；导入只校验权重和，负值不拒绝、不诊断 |
| 3 | IK `mix` | **添加布尔记录是否开启** → **D35**：新增 `enabled`（`mix != 0`），保留 `mix` 原值 |
| 4 | Transform 语义 | **自洽方案** → **D36**：4×mix + 6×offset **原值承载、不解释语义**；求值器在 V3 取证完成前不得读取计算 |
| 5 | region 计数口径 | **已关闭（2026-10-22）**：差 2 为缺图集单位未进入映射；另 4 的差根因＝查找键 `path ?? name` 对 **4 条尾随空格附件名**未命中（`docs/mapping-spine32-to-native.md` U4、`docs/skeleton/ACCEPTANCE.md` 均记）；三处数字各自口径一致，**不影响建模/实现**（TBD-E 关闭） |

## 3. A5 fixture 与验证

生成与自校验命令：

```bash
node tools/skeleton-probe/make-fixture.mjs --verify
```

实测（`strictEof: true`，旧项目 Reader 作对照解析，**不是产品读取器**）：

| fixture | 结果 | 说明 |
|---|---|---|
| `06-minimal-mesh` | ok | 非加权 mesh，含 `hull` / `edges`（不落 canonical，D31） |
| `07-weighted-mesh` | ok | 顶点 0 含负权重 `-0.2` 与 `1.2`（和为 1.0）→ 产品**必须接受** |
| `08-linked-mesh` | ok（2 warn） | 父 mesh + 2 个 linked（`inheritDeform` true/false）；warn 是**旧 Reader 局限**（解析时 default skin 尚未 push），不是 fixture 错 |
| `09-mesh-bad-triangle` | ok（旧 Reader） | 产品**必须拒绝**（`MESH_TRIANGLE_INDEX_OUT_OF_RANGE`） |
| `10-mesh-bad-bone-index` | ok（旧 Reader） | 产品**必须拒绝**（`MESH_WEIGHT_BONE_UNRESOLVED`） |
| `11-constraints` | ok | IK `mix=1` ⇒ `enabled=true`；transform 10 个 float 原值透传 |

写入路径：`docs/skeleton/fixtures/`，`fixtures.json` 同步更新为 11 项。

**发现并修正的生成器缺陷**：`nonessential` 时骨骼末尾还有 4 字节颜色
（`binary32.ts:133` 读掉但不消费），首版 fixture 少了这段 → 回读越界；已修并复跑通过。
这也说明「合成 fixture + strictEof 回读」确实能钉住布局细节。

## 4. 提案要点（A2 / A3，供审阅）

- 附件仍是 `SkinEntry` 内嵌值对象、**不单独分配 ID**；LinkedMesh 用
  `(slotId, attachmentName)` 组合键引用父、**不复制几何**。
- 约束是骨架级对象，**有稳定 ID**；`OwnerKind` 新增 `kIkConstraint` / `kTransformConstraint`。
- `hull` / `edges` **不落 canonical**（D31），导入计入 `report.unsupported`。
- boundingbox **占位不实现**（D32）。
- v3 = v2 + 增量：`formatVersion` / `minimumReaderVersion` → 3；新增
  `Skeleton.ikConstraints[]` / `transformConstraints[]`（键必须存在，可为空数组）；
  **不新增 capability**（D29）；`migrateV2ToV3()` 为纯函数、不生成 ID、可重复执行。
- 新增诊断码：`MESH_VERTEX_GEOMETRY_INVALID` / `MESH_TRIANGLE_INDEX_OUT_OF_RANGE` /
  `MESH_UV_OUT_OF_RANGE` / `MESH_LINKED_PARENT_UNRESOLVED` / `MESH_WEIGHT_SUM_INVALID` /
  `MESH_WEIGHT_BONE_UNRESOLVED` / `MESH_BINDING_COUNT_INVALID` /
  `CONSTRAINT_TARGET_UNRESOLVED` / `CONSTRAINT_CHAIN_EMPTY`；非有限浮点**复用**
  `SKELETON_FLOAT_NON_FINITE`（只追加，不改既有码语义）。

## 5. 待负责人确认 / 后续项

| 项 | 说明 |
|---|---|
| **v3 错误码命名** | v3 提案 §6.1 第 3 项我按 v2 冻结稿写作 `kFormatTooNew`，但 v2 正文另一处写 `kCapabilityUnsupported` —— 需裁定统一，我不再自行取舍 |
| ~~TBD-E~~ | ✅ **已关闭（2026-10-22）**：差 4 根因＝查找键 `path ?? name` 对 4 条尾随空格附件名未命中；三处数字各自口径一致，不影响建模/实现 |
| **V1（UV V 方向）** | A2 只承载原值、不声明方向；需单独取证 |
| **V3（动画时基）** | 阻塞 deform / 动画建模，也阻塞 transform 语义解释（D36） |
| **08 的父引用解析** | 旧 Reader 找不到父是其实现局限；产品应能解析 —— **尚未被任何实现验证**（阶段 B 验证项） |
| **09 / 10 的拒绝行为** | fixture 只给字节；产品必须拒绝 —— 同上，阶段 B 验证 |

## 6. 停点

阶段 A 到此停下（Task 5 §3 A6）。**阶段 B 未开工、也未改动任何产品代码**：
本阶段只产出文档、探针与合成 fixture，未触碰 `src/` / `importers/` / `tests/`
（工作区里 `spine32_import_plugin.cpp` / `test_spine32_plugin.cpp` 的改动是 **Task 4 B6 遗留返修**，
第二批关键词 `git diff` 零命中，见验收记录 §3）。

**A6 验收已完成（2026-10-22）** → `Agent_Task/Task5_A6_Acceptance.md`：**阶段 A 验收通过**，
附开工前置 **R1**（B6 遗留改动须先建基线提交）。是否进入阶段 B 待负责人批准 ——
本对话负责验收与计划，**不承担实现**。
