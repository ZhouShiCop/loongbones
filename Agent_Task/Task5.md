# TASK 5：Spine 3.2.01 第二批附件与约束的 canonical 建模与导入

- 状态：`§9 五个 TBD 已于 2026-10-22 由负责人裁定`（裁定已写入 `DECISIONS.md` **D29–D33**，逐项理由见 §9.1–§9.5）；**阶段 A 于 2026-10-22 获批开工**：**A1 取证已完成** → 交付报告 `Agent_Task/Task5_A1_Report.md`、详细证据 `docs/skeleton/mesh-constraint-evidence.md`（Full 1113 / Primary 963 双口径，解析失败 0）；**A1 已于 2026-10-22 复核并返修**（数字口径混用 5 处已改；T-C1 顶点空间结论方向已更正为「N2 维持：骨骼局部坐标」，判据对照实验见 `tools/skeleton-probe/tc1-space-control.mjs`），复核记录 `Agent_Task/Task5_A1_Review.md`；**A1 待裁定 5 项已全部答复**（顶点空间＝「补上验证」→ 对照实验复跑逐位一致、N2 维持；负权重 **D34**、IK 布尔 **D35**、transform 自洽 **D36**、region 口径差列为 **TBD-E**）；**A2 模型提案已产出** `docs/skeleton/mesh-and-constraint-rfc.md`、**A3 格式 v3 提案已产出** `docs/format/native-project-v3-mesh.md`、**A4 部分完成**（`field-mapping.md` §11 / `skeleton-domain-rfc.md` §10.1–§10.2 已同步；`DataType_Report.md` §3「去向」列已按 D29–D33 修正，F7 关闭）；**A1 的类型字节直接普查（TBD-F）已关闭**：新增 `tools/skeleton-probe/type-byte-census.mjs`，Full/Primary 解析失败 0，结论 **类型 4（linkedMesh）= 0 条、类型 5（weightedLinkedMesh）= 2 条**，父均为类型 3（详见 `Task5_A1_Review.md` §10）；**A5 合成 fixture 已完成并复核通过**（当前 **16 个**：5 个首批 Task 4 + 11 个第二批）：golden 值逐值对拍 ✅；TBD-F 硬要求「linked fixture 必须覆盖类型 4」已落实 ✅（08 = 类型 4、父为非加权 mesh）；首轮 1 项事实错误已就地更正（06 的 `nonessential` 形态被误称「与语料 20,250 条 mesh 一致」，实测语料 nonessential=1 仅 4/1113、`edges` 出现率 67/20,250）；**2026-10-22 按负责人裁定补齐 5 个（12–16）**：`12-mesh-bad-weight-sum-zero`（权重和 = 0）/ `13-mesh-bad-weight-sum-negative`（和 = −0.7）⇒ 判据收紧为「**和必须有限且 > 0**」，记 **D37**；`14-linked-mesh-implicit-skin`（类型 5 + `parentSkin = null`）⇒ **与语料 2 例逐字段对齐**，与 08（语料 0 例的类型 4）并列覆盖两条解析路径，记 **D38**；`15-ik-mix-zero`（`mix = 0` ⇒ `enabled = false`，D35 的 false 分支）；`16-minimal-mesh-plain`（`nonessential = 0`，语料 **1109 / 1113** 的主流形态）。语料侧取证（`weight-sum-census.mjs`，1113/1113 单位、加权顶点 312,083）：`sum ≤ 0` 与非有限**均 0 例** ⇒ D37 的拒绝路径只有合成样本；复核记录 `Agent_Task/Task5_A5_Review.md` §8，预期结果登记在 `docs/skeleton/fixtures/README.md`；**阶段 A 六项全部完成** → 交付报告 `Agent_Task/Task5_A_Report.md`；**A6 阶段 A 已验收通过（2026-10-22）**：验收记录 `Agent_Task/Task5_A6_Acceptance.md` —— 交付物齐全、内部一致性 5 项全过、停点纪律未破坏（产品代码改动仅 Task 4 B6 遗留 2 文件 69 行，第二批关键词零命中）；**开工前置 R1**：B6 遗留改动须先建基线提交（工作区仍停在 2026-10-21 MVP initial commit）。**阶段 B 实现未开工、待负责人批准**；本对话只做验收与计划，实现由实现会话执行
- 依赖：Task 4 阶段 A/B（**B0–B6 已于 2026-10-21 全部复核通过**）、Task 3（v1 codec）、Task 1/2；`项目规范.md` §5 Phase 2 与 §7/§8；`DECISIONS.md` D13/D24/D25/D26/D27/D28、V2/V3；ADR-0003；`docs/skeleton/field-mapping.md`、`docs/skeleton/skeleton-domain-rfc.md`、`docs/skeleton/plugin-host-rfc.md`、`docs/format/native-project-v2-skeleton.md`、`Agent_Task/DataType_Report.md`
- 目标项目：`D:\Works\Spine\Loongbones`
- 输入语料：`D:\Game\Other\sdorica\working\full_export\assets\game\character\spinedata`（**只读**；Full 1113 / Primary 963 双口径，不得混用分母）
- 本 Task 目标：把 Task 4 里「只计数 + warning」的第二批静态数据 —— **Mesh / WeightedMesh / LinkedMesh / BoundingBox / IK 约束 / Transform 约束** —— 建模进 canonical、纳入原生格式，并走通「导入 → 保存 → 重开 → 逐项比对」闭环；仍不支持的部分继续显式诊断，不得静默丢失。

---

## 1. 目标与边界

Task 4 的交付边界是**「结构可读」**：只有骨架元数据、Bone、Slot、Skin、RegionAttachment、Atlas/Page/Region、TextureAsset 进入 canonical。语料里第二批静态数据至今**只遍历计数、读完即弃**（`DataType_Report.md` §3 实测）：

| 能力 | 语料实测（Full） | Task 4 去向 |
|---|---|---|
| Mesh 附件（非加权） | 2,979 | 只计数 + warning |
| WeightedMesh（含权重绑定） | 17,271 | 只计数 + warning |
| LinkedMesh | 2 | 只计数 + warning |
| IK 约束 | 2,707 | 只计数 + warning |
| Transform 约束 | 208 | 只计数 + warning |
| BoundingBox 附件 | 0 | 待验证 |
| Path 约束 / Clipping 附件 | 0 | 待验证 |

`项目规范.md` §5 Phase 2 的第二条任务明确要求导入插件按「骨架、骨骼、插槽、皮肤、region、**mesh、weighted mesh、IK** 等能力逐级映射到 Loongbones canonical model」。Task 4 只完成了 region 首批，本 Task 补齐第二批。

### 1.1 为什么先做第二批静态数据，而不是动画

1. **规范的分期顺序**：mesh / 加权 mesh / IK 的**映射**属 Phase 2；动画求值与时间轴属 Phase 3，变形/蒙皮/IK 求解属 Phase 4。
2. **动画被未决项阻塞**：`DECISIONS.md` **V3（动画时基 / 帧率 / 舍入规则）** 仍未定 —— Spine 3.2.01 二进制头**无 fps 字段**，旧项目写死 30 是未经验证的假设，`项目规范.md` §3.2 明确禁止沿用。第二批静态数据不依赖时基，可先行。
3. **规模与风险**：动画时间线 4,988,424 条，是最大缺口，按「先冻结小契约、逐级递增」原则不宜与静态附件混做（`项目规范.md` §11 风险表第一条）。

> 负责人若裁定动画优先，本 Task 应让位；但 **V3 必须先有证据结论**，否则动画契约无法冻结。

### 1.2 明确不在本任务

- **动画与时间线**：全部 rotate/translate/scale/shear/attachment/color/deform/drawOrder/event 时间线、事件定义（Phase 3，被 V3 阻塞）。
- **求值 / 求解**：mesh 蒙皮求值、IK 求解、变形求值、约束求解顺序执行（Phase 4）。**本任务只建模静态数据**。
- **Path 约束 / Clipping 附件**：语料 0 例，无真实样本可验证；本任务只更新诊断与支持矩阵，不建模。
- **BoundingBox 附件**：按 **D32 保留类型占位，但不实现**（解析 / 映射 / 校验 / 导入都不做），等有真实语料或功能需求时再写。
- Qt GUI、渲染 / 视口、DragonBones、Spine Runtime 移植或翻译、修改原始 Sdorica 文件、提交任何游戏资产。
- **共享图集布局**（B6 已知缺口）：按 **D33 不并入本任务**，另立 `Agent_Task/Task6.md`。
- **回写 / 回填 Spine 格式**：按 D31 明确不提供，因此不为回写保真保留任何源字段。

### 1.3 三条不可动摇的表述边界

1. **「数据能导入」≠「能渲染网格」**：本任务不产出任何渲染结果，不得把顶点/UV/权重可导入表述为可渲染、可蒙皮、可解 IK。
2. **不得静默丢失**：本批之外的能力（动画 / deform / 事件 / path / clipping / 图集 `split`/`pad`）继续只计数 + warning，且必须进 `report.unsupported` 账本。
3. **不得伪造空对象**：证据不足的字段保持未决 + 显式诊断，不得填默认值或空数组冒充已映射。

---

## 2. 阶段 A 必须先关闭的取证项（否则契约无法冻结）

阶段 A **只做只读取证与契约提案**：不实现 Domain 类型、不改已冻结的 v2 schema、不写插件代码。

| 编号 | 待验证项 | 为什么必须先取证 | 证据要求 |
|---|---|---|---|
| **T-C1** | **加权 mesh 的顶点空间与绑定姿势** | `DECISIONS.md` **N2** 只记录「非加权 mesh 顶点是**槽骨骼局部坐标**」，**不得沿用**到加权 mesh；弄错会「动画一播就散」 | 只读语料统计 + 合成 fixture，输出数值判据，区分加权/非加权。**A1 结论（2026-10-22 复核后）**：加权顶点每条 `(bone, x, y)` 是**对应骨骼的局部坐标**，非加权为槽骨骼局部坐标 ⇒ **N2 维持**（原「推翻 N2」结论因判据无效已撤回），建模按此执行 |
| **T-C2** | **权重归一化规则与权重和分布** | 权重和不为 1 会导致蒙皮撕裂；需确定是「存储原值 + 求值归一化」还是「导入即归一化」 | 语料权重和分布直方图 + 越界/NaN 计数 |
| **T-C3** | **LinkedMesh 语义**（`parent` / `inheritDeform` / 跨 skin） | 语料仅 **2 例**，无往返验证样本（D13：不得以自造样本宣称真实兼容） | 2 例逐字节记录 + 合成 fixture；报告中标注「无真实往返样本」 |
| **T-C4** | **`hull` / `edges` 是否落 canonical** | `field-mapping.md` §5.2 提案为「编辑器辅助，不落字段」 | 语料出现率统计 + 是否可再生的论证 |
| **T-C5** | **BoundingBox 的 canonical 归属** | 语料 **0 例**，无法确定顶点数/颜色语义 | 按 **D32**：只读统计确认 0 例并登记原因即可，**保留类型占位、不实现**；支持矩阵写「待验证」，合成 fixture 不作真实兼容证据 |

### 2.1 取证项关闭结论（2026-10-22）

| 编号 | 结论 | 裁定 / 证据 |
|---|---|---|
| **T-C1** | **已关闭**：加权顶点每条 `(bone, x, y)` = **对应骨骼局部坐标**；非加权 = **槽骨骼局部坐标** ⇒ **N2 维持**（原「推翻 N2」因判据无效已撤回） | 对照实验复跑逐位一致（`Task5_A1_Report.md` §3）；负责人答复「补上验证」；模型提案 §4 / §5 |
| **T-C2** | **已关闭**：权重**保留原值**，导入只校验权重和**有限且 > 0**（`sum = 0` 或 `< 0` 均拒绝）；**单条负权重合法**（语义＝该绑定对顶点无影响） | D30 + **D34** + **D37**；\|sum−1\| ≤ 7e-5；负权重 86（Full）。⚠️ 语料实测（`weight-sum-census.mjs`，1113/1113、加权顶点 312,083）：`sum ≤ 0` 与非有限**均 0 例** ⇒ 该拒绝路径**只有合成样本**（12 / 13），不得称「语料已验证」 |
| **T-C3** | **已关闭（结构层）**：2 例逐条记录；canonical 用 `(slotId, attachmentName)` 引用父附件、**不复制几何**；**无真实往返样本**（D13），合成 fixture 不作真实兼容证据 | 证据 §3；模型提案 §6 |
| **T-C4** | **已关闭**：`hull` 100% 出现但 45.5% ≠ 顶点数（确为凸包顶点数）、`edges` 0.33% ⇒ 按 **D31 不落 canonical** | 证据 §4 |
| **T-C5** | **已关闭**：boundingbox **0 例**（Full / Primary 双口径）；按 **D32 保留类型占位、不实现** | 证据 §5 |

已由 B4 解决、本任务**不得推翻**的证据：加权顶点的 `boneCount` / `boneIndex` 为 **float（非官方 varint 写法）**，已由语料证伪并修正；mesh `uvs` 是 **regionUVs（region 相对 0..1）**，语料越出 `[0,1]` **0 例**；`triangles` 是顶点索引、越界 **0 例**。

---

## 3. 阶段 A：契约与证据（完成后停下等待审阅）

### A1 语料取证（只读，不写产品代码）

1. 统计第二批全部字段的分布与边界：顶点数上限、每顶点权重个数上限、`bones[]` 索引越界、权重和、`hull`/`edges` 出现率、IK 链长度与目标骨合法性、transform 约束各 mix 取值域。
2. 复核 **path 约束、clipping 附件、boundingbox 附件确为 0 例**（Full 与 Primary 双口径分别报告）。
3. 非有限浮点（NaN / ±Inf）扫描，沿用 `field-mapping.md` §12 的强制拒绝规则。
4. 结果写入 `docs/skeleton/`（新增或修订现有映射文档），**只含统计与元数据**，不复制语料。

### A2 canonical 模型 RFC 增补

在 `docs/skeleton/` 下新增第二批提案（如 `mesh-and-constraint-rfc.md`），至少定义：

1. `MeshAttachment` / `WeightedMesh` / `LinkedMesh`（`parentName`、`inheritDeform`）/`BoundingBoxAttachment` 的字段、**单位**、稳定 ID 与稳定键（D25：一律宿主随机 ULID；源下标只在单次导入内映射）。
2. `IkConstraint`（`boneIds` 有序链、`targetBoneId`、`mix`、`bendPositive`）与 `TransformConstraint`（`boneId`、`targetBoneId`、4 个 mix、6 个 offset）的字段与校验规则。
3. 与既有 `Skin` / `SkinEntry` / `Slot` 的关系：附件种类扩展**不得改变** `RegionAttachment` 的既有语义与字段（只增不改，规范 §4.2）。
4. 校验与诊断：新增诊断码采用稳定大写域前缀 `MESH_*` / `CONSTRAINT_*`（规范 §10.3），**只追加、不改既有码语义**；复用 `SKELETON_FLOAT_NON_FINITE` 处理非有限浮点。
5. 每个字段注明：**证据来源 → 转换规则 → golden 值计划**；证据不足的字段标注 `TBD`，不得伪装为既定事实（规范 §8.5）。

### A3 原生格式扩展（已裁定 **D29**：直接增量改结构，升 v3，不做 capability 协商）

1. `formatVersion` 升到 **3**：第二批类型（`MeshAttachment` / `WeightedMesh` / `LinkedMesh` / `IkConstraint` / `TransformConstraint`，以及按 D32 只占位的 `BoundingBoxAttachment`）**直接写进骨架 schema**，不引入 `loongbones.mesh.v1` 之类的能力声明。
2. 迁移链 **v1 → v2 → v3**：复用 B3 既有 `migrateV1ToV2()`，新增纯函数 `migrateV2ToV3()`（不生成 ID、不改旧字段、不碰磁盘、可重复执行）；**写出统一为 v3**。
3. 旧 Reader 行为：v1 / v2 Reader 遇 v3 **固定返回 `FORMAT_VERSION_UNSUPPORTED`**（与 D24 对 v2 的处理一致），不做降级打开、不静默丢字段。
4. 未知字段锚点：为新增对象定义 `OwnerKind`（骨架附件 / 约束），沿用 D22「`(kind, 稳定 ID)`」的位置无关锚点；解析到未知附件类型 / 未知约束种类时**显式拒绝 + 可定位诊断**。
5. fixtures：保留 B3 的 v2 fixture 作为**历史 fixture**，新增 v3 fixture 与 v2→v3 迁移前后语义比较（规范 §4.3、§4.6）。
6. v1/v2 的**已冻结字段语义不得顺带改写** —— 本次是结构增量扩展，不是重新设计 v2。

### A4 支持矩阵与诊断同步

更新 `docs/skeleton/field-mapping.md` §11.2/§11.3 与 `docs/skeleton/skeleton-domain-rfc.md` §10.2：把本批能力从「不支持且有诊断」移到「实现范围」；动画 / deform / 事件 / path / clipping 维持「不支持且有诊断」。更新 `DECISIONS.md` 待验证表（T-C1…T-C5）与 `Agent_Task/DataType_Report.md` 的待实现清单。

### A5 合成 fixture（至少 4 个，不含游戏资产）

最小 mesh、加权 mesh、linked mesh、非法权重/索引（畸形引用）。放 `docs/skeleton/fixtures/` 并登记其 README，标注预期结果与「哪些只是合成、无真实样本」。

> **TBD-F 结论带来的硬要求（2026-10-22）**：语料里**类型 4（linkedMesh）= 0 条**，
> 只有 2 条类型 5（weightedLinkedMesh）。因此 linked 的 fixture **必须显式覆盖类型 4**
> （父为非加权 mesh），否则非加权 linked 这一路将**完全没有测试样本**——
> 2 条真实样本只覆盖「父为 weightedMesh」，不能代替。

### A6 阶段 A 验收与停点

- 交付：取证报告、模型 RFC 增补、格式扩展提案、更新后的支持矩阵与诊断清单、合成 fixture、风险与待决项；`DECISIONS.md` 只登记**已由负责人确认**的裁定。
- 停点：**获批前禁止**新增 Domain 类型、修改 `format/` 持久化的已冻结字段、开始插件实现。
- 若 A1 取证结果与现有提案冲突（例如 T-C1 推翻 N2 的外推），**先修提案再申请批准**，不得边写代码边固化格式（Task 4 §3 同款纪律）。

---

## 4. 阶段 B：实现（阶段 A 获批后）

> **详细实施方案见 `Agent_Task/Task5_Implementation_Plan.md`**（2026-10-22 提案，**待负责人批准**）：
> 开工前置、逐包改动文件与验收判据、测试矩阵下限、数字纪律、已知坑与规模估算。本节保留工作包边界，两处一致。

仅按获批 RFC 实施；契约变更需另行批准。

### B1 Domain 扩展

- 在 `domain/` 新增第二批类型与校验，校验判据集中在既有 `validateSkeleton()` / `Project::adoptData()` 路径，与将来的写 API 共用；Domain 不依赖 RapidJSON、Spine Reader、Qt 或文件系统（Task 4 §3.1）。
- 新增诊断码只追加；既有骨架测试（core 116 / format 70）**不得回归**。
- 注意：`ProjectData` / `ProjectDocument` 布局变更后必须**全量重编**，增量链接会造成 ODR 违例（表现为堆损坏）—— Task 4 B3 已踩过。

### B2 原生格式扩展

- 按 **D29** 实现 **v1 / v2 / v3 三版本读取 + 统一写 v3**；迁移必须是**纯函数**（不改旧字段、不生成 ID、不碰磁盘、可重复执行）。
- 覆盖：含第二批数据的 v3 文档读写往返、未知字段 round-trip、锚点在重排后仍归属原对象、宿主删除后丢弃记录；**v1 / v2 Reader 遇 v3 固定返回 `FORMAT_VERSION_UNSUPPORTED`**（不降级打开）。

### B3 Reader / Mapper 扩展

- B4 Reader 已能**遍历** mesh / 约束数据（当前读完即弃）；本步把它们落地为源 IR → canonical。**不得重造 B4 已验证的部分，不得放宽其 strict EOF / 边界 / 索引断言**。
- **`hull` / `edges` 按 D31 不解析进 canonical**（遇到则计入 `report.unsupported` 账本，不建对象）。
- **BoundingBox 按 D32 不实现**：解析到该类型时产出「不支持 + 待验证」诊断，不建 Domain 对象、不留半成品。
- 加权顶点按 A1 证据实现（T-C1/T-C2），不得沿用官方 runtime 的写法假设。
- region 查找键仍是 `path ?? name`（B5 规则），未命中策略不变：`ATTACHMENT_REGION_DEGRADED`（warning）/ `ATTACHMENT_REGION_UNRESOLVED`（error）。

### B4 插件与报告

- `prepare()` / `commitImport()` 的事务顺序、回滚范围、授权根校验**不得因本任务改变**（B6 语义）。
- 重复导入仍创建**新 ID 副本**（D25）；不提供插件版本迁移钩子（D28）。
- `report.unsupported` 账本中本批条目应下降，未纳入的能力条目继续存在。

### B5 语料闭环与回归

- Full 1113 / Primary 963 双口径分别报告，**不得混用分母**（D12）。
- 至少 3 个特征不同的真实样本完成 `Spine source → Domain → Native save → reopen → semantic comparison`，顶点/UV/三角形/权重/约束参数逐项比对，记录浮点容差与已知差异。
- 全量语料扫描用于覆盖率报告，**不代表所有样本必须零 warning**。

---

## 5. 测试与验收

阶段 B 至少包括：

- 合法 mesh / 加权 mesh / linked mesh / IK / transform 约束的字段级断言与 golden 数值 + 容差表（规范 §8.4）。**BoundingBox 按 D32 不在测试范围内**（只验证「遇到该类型产出不支持诊断、不建对象」）。
- 拒绝路径：三角形索引越界、`bones[]` 索引越界、权重和非法、权重个数超限、非有限浮点、IK 链为空/自环/目标越界、transform 约束骨骼越界、linked mesh 父附件不存在。
- 格式层：第二批数据的 encode → decode → encode 语义往返；未知字段 round-trip；旧 Reader 对未知 required capability 的拒绝；保存原子性。
- 失败原子性：故意注入解析/映射失败，验证目标工程前后快照一致、无半成品实体或临时资源（B6 已有断言不得回归）。
- 端到端：`prepare → commit → reopen` 后第二批对象计数与关键字段逐项一致。
- 从**干净构建目录**运行全部既有测试与新增测试；报告精确命令、工具链、总数/通过/失败/跳过、退出码、编译警告（规范 §4 第 6 条、Task 4 §4）。**不得只运行旧二进制**。
- 退出码口径沿用 B6 约定：**只有 0 可作验收证据**；不得把已知缺口包装成通过。
- 新增/改动的验证工具必须提供就近 README 并登记根 `STRUCTURE.md`（规范 §8.2）。

---

## 6. 允许与禁止修改范围

**阶段 A 允许**：`docs/skeleton/`（新增/修订契约与取证文档）、`docs/format/`（**提案**，不改已冻结的 v2 骨架冻结稿）、`DECISIONS.md`、`Agent_Task/Task5.md`、`STRUCTURE.md`、`项目规范.md` 的能力矩阵/风险表、只读探针及其 README/登记。

**阶段 B 获批后允许**：阶段 A 文档、`domain/` 第二批类型、`importers/spine32/`、`format/` 经裁定扩展、合成 fixture、相关测试/CMake/工具 README/`STRUCTURE.md`。

**始终禁止**：动画时间线与求值；IK / 变形 / 蒙皮求解；渲染、Qt GUI、视口；DragonBones；复制或提交 Sdorica 原始资产；移植或翻译 Spine Runtime 源码；未经批准扩张原生格式；破坏 Task 1/2/3/4 的公共契约与已复核断言；把「可导入」表述为「可渲染/可播放」；顺带改写无关代码或全仓格式化。

---

## 7. 交付与状态（停止条件）

- **阶段 A**：取证报告、模型 RFC 增补、格式扩展提案、支持矩阵与诊断更新、合成 fixture、决策与风险更新。完成后**停下等待负责人审阅**。
- **阶段 B**：Domain 与格式实现、Reader/Mapper 扩展、测试与语料闭环、README/`STRUCTURE.md` 登记、完成报告与下一阶段建议。完成后**停止等待复核**；不得自行宣称视觉/渲染效果通过（规范 §7：视觉判断留给用户）。
- 一个工作包的批准**不自动**授权后续工作包（Task 4 阶段 B 的既有纪律）。

---

## 8. 交接与已知缺口（不属于本任务，需单独批准）

| 缺口 | 现状 | 需要什么 |
|---|---|---|
| **共享图集布局** | B6 遗留：语料 `h0164s5/watch/` 下 2 个单位共用与目录同名的 `.atlas.txt` → `IMPORT_RESOURCE_MISSING` | **已按 D33 另立 `Agent_Task/Task6.md`**：给 `IResourceResolver` 增加**目录列举**能力（宿主契约变更） |
| **动画与时间线**（4,988,424） | 只计数；被 `DECISIONS.md` **V3 时基**阻塞 | 先关 V3，再立 Phase 3 任务 |
| **事件 / 事件时间线** | 事件定义 1、时间线 0 | 随动画任务一并处理 |
| **Path 约束 / Clipping / BoundingBox** | 语料 0 例 | 无真实样本 → 维持「待验证」，不得宣称支持（D13） |
| 待验证项 T-A1、T-A2、T-B1、T-B2、T-B4、T-B5 | 沿用 `skeleton-domain-rfc.md` §11.1 | 在对应阶段关闭 |

---

## 9. 待负责人裁定（TBD，不得由 Agent 自行定案）

下表是**结论摘要**；逐项的「背景 / 选项 / 代价 / 建议理由 / 影响文件」见 §9.1–§9.5。
负责人未裁定前，实现按「建议」列执行并在报告中显式标注为**待追认**；不得把建议写成已定案。

| # | 问题 | 建议 | 主要风险 |
|---|---|---|---|
| # | 问题 | **负责人裁定（2026-10-22）** | 决策号 |
|---|---|---|---|
| **Q5-1** | 第二批进 v2（新增 required capability）还是升 v3？ | **直接增量修改结构，升 v3；不做 capability 协商层**（旧 Reader 遇 v3 固定拒绝） | D29 |
| **Q5-2** | 权重导入即归一化，还是保留原值 + 求值归一化？ | **B：保留原值，导入时校验和合法，归一化由求值侧负责** | D30 |
| **Q5-3** | `hull` / `edges` 是否落 canonical？ | **A：不落**；本项目使用独立 runtime，**不提供回写/回填 Spine 的方案**，无需为回写保真 | D31 |
| **Q5-4** | BoundingBox 语料 0 例，是否仍建模？ | **留空：保留类型占位，但不实现解析与映射**，待有真实语料/功能需求时再写 | D32 |
| **Q5-5** | 共享图集布局（2 个单位）是否并入本任务阶段 B？ | **先 A：不并入**；单独立项为 **`Agent_Task/Task6.md`** | D33 |

### 9.1 Q5-1：v2 + capability，还是升 v3？

> **裁定（D29，2026-10-22）：直接增量修改结构，升 `formatVersion = 3`；不做 capability 协商层。**
> 负责人理由：capability 门控只是让旧 Reader「优雅报错」，**并未解决「数据类型结构更新后解析器就是读不了」这一核心困境**；
> 当前没有已发布的存量工程、也没有不可修改的部件，因此**大改现在可行**，不必为兼容绕道。
> 下方 A/B 对照保留，仅作取舍记录。

**背景**：D24 已裁定骨架走原生格式 v2，且含 `skeletons[]` 的 v2 必须声明 `loongbones.skeleton.v1` 为 **required**（能力拒绝与版本拒绝**分别**判定）；B3 已实现 v1/v2 双版本读取与纯函数迁移。第二批是在**已有骨架对象内部**新增附件种类与约束集合，不是新增顶层结构。

**选项**

| | A：`formatVersion` 保持 2，新增 required capability `loongbones.mesh.v1` | B：升 `formatVersion = 3`，新增 v2→v3 迁移 |
|---|---|---|
| 旧 Reader 行为 | 读到未知 required capability → **能力拒绝**（语义精确：知道是"我不懂 mesh"） | 读到 `3` → `FORMAT_VERSION_UNSUPPORTED`（更粗：连骨架一起拒） |
| 新增迁移函数 | 不需要 | 需要（+ 旧版本 fixture + 迁移前后语义比较 + 降级策略，规范 §4.3） |
| 版本膨胀 | 无 | 每加一类能力就要升版本，与规范 §4.2「优先只增不改」相悖 |
| 代价 | 同一版本号的文档存在两种形态，Reader 必须**靠 capability 而非版本号**判断 | 语义更直白（版本 = 能力集合） |

**为什么建议 A**：v1→v2 之所以**必须**升版本，是因为骨架是结构级新增（顶层 `skeletons` 数组，旧 Reader 完全看不懂）；而第二批是已有对象内的可选扩展，旧 v2 Reader 遇到未知 required capability 会明确拒绝，不会误读 —— 这正是 D24 已经确立的「能力声明」机制的用法。

**必须同时裁定**：该 capability 是 required 还是 optional。建议与 `skeleton.v1` 同规则 —— **文档中存在 mesh 类附件或约束时必须声明为 required**。

**影响文件**：`docs/format/native-project-v2-skeleton.md`（或新增 v3 RFC）、`docs/skeleton/mesh-and-constraint-rfc.md`、`format/` 读写与迁移、`tests/format/`、fixtures。

### 9.2 Q5-2：权重归一化放在导入还是求值？

> **裁定（D30，2026-10-22）：选 B —— 保留源原值，导入期校验权重和合法，归一化由求值侧负责。**
> 本任务不改写源语义；`sum = 0` / NaN / 越界在导入期即拒绝。

**背景**：Spine 加权 mesh 的权重语义上是归一化的（每顶点权重和 = 1），但浮点求和存在误差，且**语料尚未统计权重和分布**（T-C2）。这决定 canonical 里存的是"源原值"还是"已归一化值"。

**选项**

| | 导入即归一化 | 保留原值 + 导入校验，求值侧归一化 | 保留原值 + 求值每次归一化 |
|---|---|---|---|
| 与源数据逐项比对 | **破坏**（重开后与源 hash/数值对不上，且不可逆） | 保留 | 保留 |
| 源语义保真 | 改写源语义 | 保真 | 保真 |
| 非法权重和的处置 | 被归一化掩盖 | 导入期明确拒绝（和 = 0 / NaN / 越界） | 推迟到求值期才发现 |
| 每帧开销 | 无 | 无 | 每次求值都要除 |

**为什么建议 B**：本任务只建模静态数据，**渲染证据在 Phase 4**。把"是否归一化"的决定权留给有渲染证据的阶段，本阶段只保证「权重和是否合法」是**可判定的**、且原值不丢，符合 §1.3「不得静默改写」。

**前置**：T-C2 必须先给出语料权重和的 min/max 与 `|sum−1| > tol` 的比例，容差据此确定；若语料里存在显著 `sum ≠ 1` 的样本，本项的结论可能需要重审。

**影响文件**：`domain/include/…/skeleton.hpp`（Mesh/WeightedMesh 字段与校验）、`docs/skeleton/field-mapping.md` §5.2、T-C2 统计、后续求值阶段的契约文档。

### 9.3 Q5-3：`hull` / `edges` 是否落 canonical？

> **裁定（D31，2026-10-22）：选 A —— 不落 canonical。**
> 负责人补充的**决定性理由**：本项目使用**独立 runtime**，且**不提供回写 / 回填 Spine 的方案**
> （也不必为将来可能不存在的回写能力预先实现 runtime 保真）。既然没有回写目标，就不需要为 `hull` / `edges` 承担保真义务。

**背景**：`field-mapping.md` §5.2 现记 `hull`「提案中不落字段（编辑器辅助）」。`hull` 是凸包顶点数（hit test / 编辑器边框用），`edges` 是边信息，二者**不参与渲染与蒙皮**。

| | A：不落 | B：落 |
|---|---|---|
| canonical 体积 | 更瘦 | 多字段，求值/渲染用不到（规范 §8.3 反对塞无关数据） |
| 可再生性 | `hull` 可由顶点算凸包、`edges` 可由 `triangles` 推边界边 | 保真，避免重算与源不一致 |
| 风险 | 未来若要**严格回写 Spine 格式**会丢字段 | 共线点等边界情况下的凸包算法可能与原 `hull` 不一致 |

**为什么建议 A**：本项目**没有 Spine 回写 / 导出目标**（规范 §2.4 已放弃 DragonBones 导出，未承诺 Spine 回写），而 `hull`/`edges` 只影响编辑器辅助显示，不影响渲染与蒙皮。落字段等于为不存在的目标提前背上保真义务。

**前置**：T-C4 需先统计二者在语料 mesh（2,979）中的出现率与取值规律（例如 `hull` 与顶点数的关系），作为"可再生"判据的证据。

**影响文件**：canonical 字段表、B3 Reader（是否解析并丢弃）、`report.unsupported` 账本。

### 9.4 Q5-4：BoundingBox 语料 0 例，是否仍建模？

> **裁定（D32，2026-10-22）：留空 —— 保留类型占位，但不实现。**
> 即：canonical 与 schema 里保留 `BoundingBoxAttachment` 的类型位置（枚举值 / 类型定义 / 诊断码），
> **不实现**解析、映射、校验与导入；等将来有真实语料或功能需求时再写。
> 支持矩阵维持「不支持 + 待验证（语料 0 例）」，不得写「已验证支持」。

**背景**：`field-mapping.md` §5.3 记 boundingbox / clipping / path 附件语料 **0 例**。BoundingBox 的源结构极简（顶点数组，无权重、无 UV），但它是**碰撞/裁剪语义**，不是渲染内容，0 样本意味着字段语义无从取证。

| | A：建模但标「待验证」 | B：本批不建模，维持「不支持且有诊断」 |
|---|---|---|
| schema 准备度 | 预留位置，将来只需补证据 | 需再走一轮 RFC |
| 风险 | 基于 0 样本设计字段（顶点数、可选颜色语义），真实样本出现时可能要改字段 → 属**破坏性变更**，须按规范 §4.5 提 RFC | 第二批看起来"只做了一半" |
| D13 约束 | 支持矩阵只能写「待验证」，**不得**写「已验证支持」 | 不涉及 |

**为什么建议 A**：结构极简 → 证据风险低；但必须同时满足三条约束：① 支持矩阵写「待验证」；② 只用合成 fixture 覆盖，且报告里写明"无真实样本可往返验证"；③ 一旦真实样本与合成 fixture 语义冲突，**以真实样本为准**并走 RFC 改字段。

**影响文件**：支持矩阵（`field-mapping.md` §11、`skeleton-domain-rfc.md` §10）、格式 capability 清单、合成 fixture 与 README。

### 9.5 Q5-5：共享图集布局是否并入本任务？

> **裁定（D33，2026-10-22）：先 A —— 不并入本任务；单独立项为 `Agent_Task/Task6.md`。**
> 本任务只负责第二批静态数据的建模与导入，宿主 `IResourceResolver` 的目录列举能力由 Task 6 承接。

**背景**：B6 遗留缺口 —— 语料 `h0164s5/watch/` 下 `h0164s5_watch_hour` 与 `h0164s5_watch_minute` 两个骨架共用与**目录**同名的 `h0164s5_watch.atlas.txt`，与插件「同名替换」规则不符 → `IMPORT_RESOURCE_MISSING` → 2 个单位 prepare 失败、smoke 退出码 **7**。修复需要给 `IResourceResolver` 增加**目录列举**能力。

| | A：不并入，单独立项 | B：并入本任务阶段 B |
|---|---|---|
| 目标纯度 | 符合规范 §8.1「一个任务只解决一个目标」 | 目标扩张（数据建模 + 宿主接口变更） |
| 风险面 | 受限 | `IResourceResolver` 属 `importers/api` **宿主契约**，改动会触及 B6 已复核的事务语义与 10 项宿主测试 |
| 代价 | 退出码 7 会一直挂到该独立项完成 | 省一次审阅往返 |

**为什么建议 A**：这是**已复核模块的接口变更**，不是数据建模工作；混进本任务会让"第二批导入"的验收与"B6 事务回归"纠缠在一起，出问题难以定位。

**无论是否并入，本任务都必须做到**：语料闭环报告里把这两个单位**单独列出**，不得让它们混进"第二批能力失败"的统计，也不得因此把退出码 7 说成通过。

**影响文件（若改为 B）**：`importers/api/include/…/import_plugin.hpp`（`IResourceResolver` 接口）、`importers/spine32/src/atlas_import.cpp`、`tests/importers/test_import_host.cpp`、`docs/skeleton/B6_REPORT.md`。
