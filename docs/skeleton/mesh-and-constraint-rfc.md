# 第二批附件与约束 canonical 模型提案（Task 5 · A2）

- 状态：**提案，待负责人审阅**（阶段 A 交付物之一；获批前不得实现，Task 5 §3 A6）
- 依赖裁定：`DECISIONS.md` **D29–D33**（2026-10-22）+ 负责人 2026-10-22 对 A1 复核的答复
  （负权重 **D34**、IK 布尔 **D35**、transform 自洽方案 **D36**）
- 证据：`docs/skeleton/mesh-constraint-evidence.md`（A1 取证）+ `Agent_Task/Task5_A1_Review.md`（复核与更正）
- 只增不改：`RegionAttachment` / `Skin` / `SkinEntry` / `Slot` 的既有语义与字段**不动**（规范 §4.2）

## 1. 范围与证据口径

| 项 | 内容 |
|---|---|
| 覆盖 | `MeshAttachment` / `WeightedMeshAttachment` / `LinkedMeshAttachment` / `BoundingBoxAttachment`（**占位**）/ `IkConstraint` / `TransformConstraint` |
| 不覆盖 | 动画（全部时间线，含 deform）、事件、path 约束、clipping —— 维持「不支持且有诊断」 |
| 语料口径 | **Full 1113** / **Primary 963** 双分母（D12），分别统计，不合并 |
| 证据复现 | `node tools/skeleton-probe/mesh-constraint-probe.mjs "<语料根>" -o tools/skeleton-probe/out`；判据对照 `node tools/skeleton-probe/tc1-space-control.mjs "<语料根>"` |
| 解析依据 | 旧项目 `converter/src/read/binary32.ts` 作**证据参照**；阶段 B 产品 Reader 独立实现 |

**命名纪律**：本文件是提案，`TBD` 一律显式标注，不把未验证的推断写成既定事实（规范 §8.5）。

## 2. 类型总览

| 类型 | 处置 | 语料规模（Full） | 本次 |
|---|---|---|---|
| `MeshAttachment` | 新增 canonical 类型 | 2,979 | 提案建模 |
| `WeightedMeshAttachment` | 新增 canonical 类型 | 17,271 | 提案建模 |
| `LinkedMeshAttachment` | 新增 canonical 类型（引用语义） | 2 | 提案建模 |
| `BoundingBoxAttachment` | **占位但不实现**（D32） | 0 | 只留类型位与诊断码 |
| `IkConstraint` | 新增 canonical 类型 | 2,707 | 提案建模 |
| `TransformConstraint` | 新增 canonical 类型（**原值承载，不解释语义**） | 208 | 提案建模 |
| `hull` / `edges` | **不落 canonical**（D31） | 20,250 / 67 | 不计入字段 |

## 3. ID 与稳定键（沿用 D25 / D22）

- 所有持久化对象 ID 一律**宿主随机 ULID**；源下标只在单次导入内作引用映射。
- 附件仍是 `SkinEntry` 的**内嵌值对象**，**不单独分配 ID**（沿用 `skeleton-domain-rfc.md` §5.4）。
- `LinkedMeshAttachment` 需要引用父附件，但父附件没有自己的 ID ⇒ 引用键用
  **`(slotId, attachmentName)`** 组合键（与 `SkinEntry` 的键同构、位置无关），
  导入时解析；解析不到即诊断（§11）。
- 未知字段锚点沿用 D22：附件 → `(kSkinEntry, entryId) + /attachment`；
  约束 → 约束对象需**有稳定 ID**（与附件不同：约束是骨架级对象，见 §8 / §9）。

## 4. `MeshAttachment`（非加权）

| 字段 | 类型 | 单位 / 值域 | 说明 |
|---|---|---|---|
| `type` | 枚举 `"mesh"` | 固定 | 与 `region` 区分 |
| `regionId` | `string` | 指向 `AtlasRegion` | 查找键仍是 `path ?? name`（B5 规则），未命中策略不变 |
| `uvs` | `float[]`（长度 = 2×顶点数） | **regionUVs，`0..1`** | ADR-0003 C5a、`DECISIONS.md` N3；整页 UV 由运行时算 |
| `triangles` | `uint32[]`（3×三角形数） | 顶点索引 | 必须 `< 顶点数` |
| `vertices` | `float[]`（2×顶点数，xy 成对） | 1 = 1 世界单位 | **槽骨骼局部坐标**（见下） |
| `width` / `height` | `float` | 世界单位 | nonessential；语料 100% 出现，缺失时默认 0 |

**顶点空间（决定性证据）**：非加权 mesh 顶点是**槽骨骼局部坐标**。
证据 = `tc1-space-control.mjs` Test C/D（复核后复跑逐位一致）：
同 slot 内 mesh 质心到 `region.x/y`（**已知**局部坐标）mean **34.766**，
到骨架原点 83.012；到 region 的**世界**位置 435.473 与到槽骨骼世界原点 418.414 同量级
（若质心是骨架空间坐标，前者应 ≈ 0）。`DECISIONS.md` **N2 维持**。

**V 方向**：`uvs` 的 V 轴方向由 `DECISIONS.md` **V1** 阻塞 —— 本提案**只承载原值**，
不声明 V 向上还是向下，相关判据留待 V1 取证（不得用"看起来对"的渲染作判据）。

校验与诊断见 §11。

## 5. `WeightedMeshAttachment`

在 §4 字段基础上增加**每顶点的骨骼绑定**，并**不落 `vertices`**：

> **不落 `vertices`**：加权 mesh 的几何**全部**在 `vertexBindings[].x / .y` 里（每条是对应骨骼的
> 局部坐标，见 §5 顶点空间）；源格式没有独立的全局顶点数组，**顶点数 = `vertexBindings.size()`**。
> （与格式提案 §3.1 一致；该表 `vertices` 对 `weightedMesh` 记为「不适用」。）

| 字段 | 类型 | 说明 |
|---|---|---|
| `vertexBindings` | `VertexBinding[][]` | 外层按下标对应顶点；内层为该顶点的绑定列表（保序，与源一致） |
| `VertexBinding.boneId` | `string` | 指向本骨架的 `Bone`；源 `boneIndex` 越界或**非整数编码**即拒绝 |
| `VertexBinding.x` / `.y` | `float` | **该骨骼局部空间**的绑定姿势坐标（见下） |
| `VertexBinding.weight` | `float` | **源原值**，导入期不改写（D30） |

**顶点空间（决定性证据）**：每条 `(bone, x, y)` 是**该骨骼自己的局部坐标**。
证据 = Test A（复跑逐位一致）：同一顶点各条目的**原始** (x,y) 最大两两距离 mean **122.132**
（>100 的 86,358/204,304），而**变换到骨架空间后** mean **5.502**（>100 仅 3,222）；
各条目完全相同的顶点仅 22/204,304。残差来自判据级近似
（72,256 根骨骼里 4,176 根 `inheritScale`/`inheritRotation` 为 false 未参与折叠），量级差 22 倍。

**权重规则**：

| 规则 | 内容 | 裁定 |
|---|---|---|
| 保留原值 | 导入期不归一化、不舍入 | D30 |
| 权重和 | 每顶点和必须**有限且非 0**；否则拒绝 | D30 |
| **负权重** | **合法**。语义＝**该绑定对顶点无影响**（不产生效果）。导入不拒绝、不诊断，原样保留 | **D34**（2026-10-22） |
| 归一化 | 由**求值侧**负责，导入期不做 | D30 |
| 容差 | 语料实测 \|sum−1\| ≤ 7.0e-5（float32 舍入量级）；导入校验取 **1e-3**，不卡死源精度 | A1 证据 |

**每顶点骨骼数**：语料 1…16（mean 1.90，2 骨 47.1%）。格式本身无上限 ⇒ canonical **不设硬上限**，
只用「不得为 0」做结构校验。

## 6. `LinkedMeshAttachment`

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | 枚举 `"linkedMesh"` | |
| `parentKey` | `{ slotId, attachmentName }` | 指向**同骨架内**的父 `MeshAttachment` / `WeightedMeshAttachment`（§3 的引用键） |
| `inheritDeform` | `bool` | 源 `deform` 位；`true` = 跟随父附件的 deform，`false` = 独立 |
| `regionId` | `string` | 与父一致（继承） |

**几何不复制**：`uvs` / `triangles` / `vertices` / 绑定数据**一律不复制**到子附件，
一律从父附件取（源格式本身如此：语料 2 条 linked 的自身顶点区为空）。
导出到第三方格式时按「L0 引用 / L1 复制并独立化」两种策略选一 —— **策略选择属后续任务，本提案不定**。

语料 2 条（Full 与 Primary 同一组）：

| 单位 | slot | 附件名 | parent | inheritDeform |
|---|---|---|---|---|
| `b3240c3/b3240c3.skel.bytes` | body | `body_no mouth` | `body` | true |
| `h0164s5/h0164s5.skel.bytes` | body | `body_turn` | `body` | false |

`inheritDeform` 的 true / false 各有一例 ⇒ 合成 fixture 之外有真实样本可对拍。

**父附件的加权性（2026-10-22 二次核对更正）**：这两条的 `parentKey` 指向的父附件 `body`
在两例中**都是加权 mesh**（旧 Reader 因解析顺序把 linked 的继承字段读出 `undefined`，
探针据此打印的 `weighted: false` **无效**，见证据文档 §3）。因此：

- 语料的 2 条 linked 属 **`weightedLinkedMesh`（类型字节 5）**，与 `DataType_Report.md`
  的「`weightedLinkedMesh` 2」一致；⇒ `parentKey` **必须支持指向 `WeightedMeshAttachment`**
  （本提案 §3 / 上表已是「父 `MeshAttachment` / `WeightedMeshAttachment`」两种皆可 ✓）。
- **不得**用「语料 2 条都是非加权 linked」来简化实现。
- **TBD-F 已关闭（2026-10-22）**：类型字节直接普查给出 **类型 4（linkedMesh）= 0 条、
  类型 5（weightedLinkedMesh）= 2 条**（Full/Primary 同），父附件类型字节均为 3。
  普查工具 `tools/skeleton-probe/type-byte-census.mjs`，解析失败 0，与 A1 计数逐位吻合。
  ⇒ 非加权 linked 在语料中**无真实样本**，合成 fixture 必须覆盖类型 4，不得只按类型 5 实现。

## 7. `BoundingBoxAttachment`（占位，不实现 —— D32）

- canonical 与 schema **保留类型位置与诊断码**，但不做解析、映射、校验与导入。
- 遇到该附件类型 → 产出「不支持 + 待验证」诊断，**不建对象、不留半成品**。
- 支持矩阵写「不支持 + 待验证」，**不得**写「已验证支持」。
- 语料 0 例；3.2.01 附件类型字节只有 0…5，boundingbox 是其中类型 1 —— 有字节值、无样本。

## 8. `IkConstraint`

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` / `name` | `string` / `string` | 骨架级对象，**有稳定 ID**（ULID） |
| `boneIds` | `string[]` | **有序链**，指向本骨架 `Bone` |
| `targetBoneId` | `string` | 目标骨 |
| `mix` | `float` | **源原值**，不做解释 |
| `enabled` | `bool` | **是否开启**（`mix != 0`）—— 见下 |
| `bendPositive` | `bool` | 弯曲方向；语料 true 1,348 / false 1,359 |

**`enabled` 的由来（D35）**：语料 `mix` **只有 0（302）与 1（2405）**，没有任何中间值
（Primary 同构：0×274 / 1×2207）。因此 canonical 增加一个布尔 `enabled` 记录**是否开启**，
同时**保留 `mix` 原值** —— 将来若出现中间值，`mix` 仍在，`enabled` 的语义也不被破坏。

校验：链长 ≥ 1；`boneIds` / `targetBoneId` 必须解析到本骨架的骨骼；`mix` 非有限即拒绝。

## 9. `TransformConstraint`

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` / `name` | `string` / `string` | 骨架级对象，**有稳定 ID** |
| `boneId` / `targetBoneId` | `string` | 受约束骨 / 目标骨 |
| `rotateMix` / `translateMix` / `scaleMix` / `shearMix` | `float` | **源原值** |
| `offsetRotation` / `offsetX` / `offsetY` / `offsetScaleX` / `offsetScaleY` / `offsetShearY` | `float` | **源原值** |

**语义处置（D36，自洽方案）**：canonical **保留原值、`mix` 与 `offset` 都不解释语义**。

理由（为什么这是"最符合目标"的选择）：

1. 目标是**忠实承载 + 可无损读回**，而不是复刻某个 runtime 的求值结果；
   解释语义在**没有证据**的前提下只会引入失真。
2. 证据显示它**不是** Spine 3.5+ 的语义：`rotateMix` ∈ [−180, 142.6]、
   `translateMix` ∈ [−152.7, 305.1]、`scaleMix` ∈ [−316.4, 175.7]，三者**从未取到 1**，
   只有 0 或其它值；反倒是 4 个 offset 落在 0…1（语料 208 条，Full）。
   按「0…1 插值因子」建模必然失真。
3. `offsetRotation` 与 `offsetX` 在语料里**恒为 0**，无样本可判定用途 ⇒ 更不能凭字段名推断。

约束：**求值器在 V3（动画时基）取证完成前，不得读取这些字段做计算**；
语义解释留待 V3 与动画任务，届时以真实样本 + 数值判据确定（禁用"看起来对"的渲染判据）。

## 10. 与既有类型的关系（只增不改）

| 既有 | 是否受影响 | 说明 |
|---|---|---|
| `RegionAttachment` | **否** | 字段与语义完全不动；mesh 是**并列的新类型**，不改写 region |
| `Skin` / `SkinEntry` | **否**（只扩 `attachment` 的取值种类） | 键仍是 `(slotId, attachmentName)`；`SkinEntry.id` 仍是 ULID |
| `Slot` | **否** | 绘制顺序、颜色、混合不变 |
| `Bone` | **否** | 只被新类型**引用**（绑定 / 约束），不改其字段 |
| `AtlasRegion` / `AtlasPage` | **否** | `regionId` 复用查找规则 `path ?? name` |

## 11. 校验与诊断

沿用三分法（`skeleton-domain-rfc.md` §8.1）；新增码采用稳定大写域前缀 `MESH_` / `CONSTRAINT_`
（规范 §10.3），**只追加，不改既有码语义**。非有限浮点**复用** `SKELETON_FLOAT_NON_FINITE`
（`field-mapping.md` §12 的强制拒绝规则）。

| 新码 | 触发 | 处置 |
|---|---|---|
| `MESH_TRIANGLE_INDEX_OUT_OF_RANGE` | `triangles` 中有索引 ≥ 顶点数 | 拒绝该候选 |
| `MESH_VERTEX_GEOMETRY_INVALID` | 顶点数为 0，或 `vertices` / `uvs` 长度不是偶数 | 拒绝 |
| `MESH_UV_OUT_OF_RANGE` | `uvs` 落在 `[0,1]` 之外 | 拒绝（语料 **0 例**，无真实样本可验证，标注 TBD-B） |
| `MESH_LINKED_PARENT_UNRESOLVED` | `parentKey` 解析不到父附件 | 拒绝（子附件无几何来源） |
| `MESH_WEIGHT_SUM_INVALID` | 某顶点权重和**不是有限正值**：`sum = 0`、或 `sum < 0`、或 NaN / ±Inf（**判据是"和"不是单条符号**，D30 + D34 + **D37**） | 拒绝（fixture `12-mesh-bad-weight-sum-zero` / `13-mesh-bad-weight-sum-negative`） |
| `MESH_WEIGHT_BONE_UNRESOLVED` | `boneIndex` 越界，或不是整数编码 | 拒绝 |
| `MESH_BINDING_COUNT_INVALID` | 某顶点的绑定列表为空 | 拒绝 |
| `CONSTRAINT_TARGET_UNRESOLVED` | IK / transform 的骨骼或目标骨解析不到 | 拒绝 |
| `CONSTRAINT_CHAIN_EMPTY` | IK 链长为 0 | 拒绝 |
| `SKELETON_FLOAT_NON_FINITE`（既有） | 上述任何 float 字段为 NaN / ±Inf | 拒绝（沿用，不新增） |

**不产出诊断**的情况（避免噪音）：负权重（合法，D34）、`hull` / `edges`（按 D31 只计入
`report.unsupported` 账本，不建对象）、boundingbox（按 D32 走「不支持 + 待验证」，用既有码）。

## 12. golden 值计划与待验证

| 项 | golden / fixture 计划 | 备注 |
|---|---|---|
| 非加权 mesh 顶点空间 | fixture `v3-mesh-min` **拆成两个**：`06-minimal-mesh`（`nonessential=1`，含 `edges` / `width` / `height`）+ `16-minimal-mesh-plain`（`nonessential=0`，**语料 1109/1113 的主流形态**）；顶点存原值，重开逐位一致 | 语义验证靠 C/D 对照实验的真实语料 |
| 加权绑定 | fixture `v3-mesh-weighted`：2 顶点 × 2 骨，权重 0.7/0.3 与 **1.2/−0.2**（负权重合法） | 负权重必须进 fixture（D34） |
| LinkedMesh | fixture `v3-mesh-linked` **拆成两个**：`08-linked-mesh`（类型 **4**，父为非加权 mesh，显式 `parentSkin='default'`，**语料 0 例**）+ `14-linked-mesh-implicit-skin`（类型 **5**，父为加权 mesh，`parentSkin=null`，**与语料 2 例逐字段对齐**）；两者 `inheritDeform` true / false 各一 | 负责人裁定：解析器两侧都要能处理（"解析器需要对齐"）；canonical 侧不受影响 |
| 畸形引用 | fixture `v3-mesh-invalid` 拆成**四个**：`09` 三角索引越界、`10` boneIndex 越界、`12` 权重和 = 0、`13` 权重和 < 0 → 预期拒绝码 | 覆盖 §11 的拒绝路径；12 / 13 判据见 D37 |
| IK / transform | fixture `v3-constraints` 拆成**两个**：`11-constraints`（`mix=1` → `enabled: true`）+ `15-ik-mix-zero`（`mix=0` → **`enabled: false`**，语料 302 例）；transform 10 个 float 原值透传 | D35 的 false 分支此前无样本；transform 语义不解释（D36） |

**待验证（不伪装成既定事实）**：

| 编号 | 项 | 责任阶段 |
|---|---|---|
| **TBD-A** | `uvs` 的 V 方向（V1）—— 本提案不声明 | V1 取证后 |
| **TBD-B** | UV 越界的真实样本（语料 0 例）⇒ 拒绝策略无法用真实语料验证 | 后续（有样本时） |
| **TBD-C** | transform 的 mix / offset 真实语义 | V3 与动画任务 |
| **TBD-D** | LinkedMesh 的 L0 / L1 导出策略 | 后续（导出任务） |
| **TBD-E** | region 计数口径 27,459 / 27,457 / 47,709 / 47,711 / 47,705 的差异 | 后续核对方可同步 `field-mapping.md` |
| ~~**TBD-F**~~ | ~~linked 附件的类型字节 4 vs 5 的直接计数~~ | **已关闭（2026-10-22）**：`type-byte-census.mjs` 直接普查 ⇒ **类型 4 = 0 条、类型 5 = 2 条**，父均为类型 3。非加权 linked **无真实样本**，fixture 需覆盖 |
