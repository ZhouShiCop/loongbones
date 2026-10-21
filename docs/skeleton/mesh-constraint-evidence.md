# 第二批静态数据语料取证报告（mesh / 蒙皮 / 约束）

> 对应 `Agent_Task/Task5.md` §2（T-C1…T-C5）与 §3 A1。
> 本文只回答**语料里到底是什么样**，不定义模型、不改产品代码。

## 0. 取证纪律与可复现性

- **只读**：探针只读取语料，不写语料、不写产品代码。
- 工具：`tools/skeleton-probe/mesh-constraint-probe.mjs`（解析复用**旧项目** `converter/src/read/binary32.ts`
  作为证据参照；产品 Reader 在阶段 B 独立实现，不以本探针为依据）。
- **双分母**（D12）：`Full` = 语料根下全部 `.skel.bytes`，`Primary` = `<dir>/<dir>.skel.bytes` 子集。
  两者分别统计，不合并、不互相替代。
- 输出只有统计量与**相对路径**；不写盘符/用户名、不写语料原文（规范 §8.2 / §9.3）。
- 语料根不在文档中（规范 §8.2），复现时由负责人提供：

```bash
# 用法见 tools/skeleton-probe/README.md
node tools/skeleton-probe/mesh-constraint-probe.mjs "<语料根>" -o tools/skeleton-probe/out
```

产出：`out/mesh-constraint-probe.md`（人读）与 `out/mesh-constraint-probe.json`（机器读）。

实测快照：**Full 1113 个单位**、**Primary 963 个单位**，解析失败 **0**。

## 1. 附件分类

| 口径 | region | mesh | weightedMesh | linked | boundingBox |
|---|---|---|---|---|---|
| Full | 27,459 | 2,979 | 17,271 | 2 | **0** |
| Primary | 24,605 | 2,572 | 15,751 | 2 | **0** |

- mesh / weightedMesh 与 B4 语料扫描（`binary-read-smoke`）的按类型字节计数一致；
  非 region 合计 20,252 与 B5 的「非 region 跳过」精确一致。
- ~~口径待核：region 27,459 vs B5 的 27,457~~ **已定位（2026-10-22 复核）**：B5 少的是 2 个
  **缺图集、未进入映射**的单位（`h0164s5_watch_hour` / `h0164s5_watch_minute`），
  27,459 = manifest 全量口径。另：`field-mapping.md` §10.1 记 47,705（4 未命中）与 B5 的 47,709、
  本探针的 47,711 还差 4，与本任务无关，A4 同步时一并处理。

## 2. 加权 mesh 顶点（T-C1 / T-C2）

### 2.1 规模与编码

| 项 | Full | Primary |
|---|---|---|
| 加权附件 | 17,271 | 15,751 |
| 加权**顶点** | **312,083** | 282,039 |
| 加权**权重条目**（每条骨骼一个，= 顶点 × 平均骨骼数） | 595,995 | 536,369 |
| 每顶点骨骼数 | 1…16，mean 1.9097 | 1…16，mean 1.9018 |
| 骨骼数分布 top | 2骨 47.06%、1骨 34.53%、3骨 13.93% | 2骨 47.2%、1骨 34.7%、3骨 13.8% |
| boneIndex 越界 | **0** | **0** |
| boneCount / boneIndex 非整数编码 | **0** | **0** |

`boneCount` 与 `boneIndex` 都是 **float32 存储**（需要 `Math.round` 才能当整数用），
与 DECISIONS.md N1 一致 —— 官方 runtime 的 varint 写法在这里必错。

### 2.2 权重归一化（T-C2）

| 项 | Full | Primary |
|---|---|---|
| 每顶点权重和 | 0.99992997 … 1.00001000，mean 0.999995 | 0.99992998 … 1.00001000 |
| \|sum − 1\| | ≤ **7.0e-5**，mean 5e-6 | ≤ 7.0e-5 |
| 权重和 = 0 的顶点 | 0 | 0 |
| 非有限权重 | 0 | 0 |
| **负权重** | **86**（min −0.03365） | **80** |

结论：语料权重**已归一化到 float32 精度**（离差 ≤ 7e-5，即 float32 舍入量级）。
D30「保留原值 + 导入期校验」成立；校验容差取 **1e-3** 足够宽松（实测离差比它小一个数量级以上）。

**新发现（需要裁定）**：语料存在共 86 个负权重（绝对值 ≤ 0.0337，且这些顶点的和仍 ≈ 1）。
「权重和合法」的校验**不能**顺带拒绝负权重，否则会误杀 —— 已裁定为 **D34**（单条负权重合法）。

> **2026-10-22 独立复验（D37 证据）**：`weight-sum-census.mjs`（不复用旧 Reader，按
> `binary_reader.cpp` 已验证布局独立走字节）全语料 **1113 / 1113** 单位、加权顶点
> **312,083**（与本节 Full 值逐位一致）复跑结果：
> `sum = 0` **0** 例、`sum < 0` **0** 例、非有限 **0** 例、`|sum−1| > 1e-4` **0** 例；
> 区间 min **0.9999299766495824** / max **1.0000100044999272**；单条负权重 **86**。
> ⇒ **D37「和必须 > 0」的拒绝路径在真实语料里没有触发样本**，只能由合成 fixture
> （`12-mesh-bad-weight-sum-zero` / `13-mesh-bad-weight-sum-negative`）覆盖，
> 阶段 B 不得把它表述为「语料已验证」。

### 2.3 顶点空间（T-C1）

> **2026-10-22 复核：本节原结论方向相反，已更正。** 原判据（到骨骼世界位置 vs 到骨架原点）
> 本身无效 —— 判据方向的正确推演是：若顶点是**骨架空间**坐标，它应当**靠近骨骼的世界位置**
> （网格长在骨骼上）；「离原点近、离骨骼远」恰恰是**骨骼局部坐标**的特征。下面先给原判据的
> 实测值，再给**对照实验**（`tools/skeleton-probe/tc1-space-control.mjs`）。

原判据实测（mean；加权侧用主权重骨骼的那条 (x,y)）：

| 距离（Full） | 非加权 mesh | 加权 mesh |
|---|---|---|
| 到**槽骨骼**世界位置 | 434.86 | 380.12 |
| 到**主权重骨骼**世界位置 | — | 384.69 |
| 到**根骨骼**世界位置 | 146.13 | 72.20 |
| 到**骨架原点** | 146.09 | 72.16 |

**对照实验（已知答案的样本，用于检验判据本身）**：

| 实验 | 对象 | 结果 |
|---|---|---|
| B（对照） | `region.x/y` —— 语义**已知**是槽骨骼局部坐标 | 到原点 mean **50.365**、到槽骨骼世界位置 mean **377.896** ⇒ 判据把已知局部坐标判成「骨架空间」，**判据无效** |
| A2 | 加权顶点同一顶点各骨骼条目的 (x,y) 变换到骨架空间后的最大两两距离 | mean **5.502**（>100 仅 3,222/204,304）⇒ 各条目**重合** |
| A1 | 同一顶点各条目**原始** (x,y) 的最大两两距离 | mean **122.132**（>100 达 86,358/204,304）⇒ 原始值彼此相差很大 |
| A3 | 各条目 (x,y) 完全相同的顶点 | 22 / 204,304 —— 若存骨架空间坐标应占绝大多数 |
| C | 同 slot 内非加权 mesh 质心 → `region.x/y`（局部） vs → 骨架原点 | **34.766** vs 83.012 |
| D | 同 slot 内非加权 mesh 质心 → region 的**世界**位置 vs → 槽骨骼世界原点 | **435.473** vs 418.414（若质心是骨架空间坐标，前者应 ≈ 0） |

**结论**：语料里的 mesh 顶点，**加权的是每条 `(bone, x, y)` 所属骨骼的局部坐标，
非加权的是槽骨骼局部坐标** —— 与 Spine 语义一致，**DECISIONS.md N2 维持**（不是被推翻）。
A2 残差不为 0 的原因是世界变换为判据级近似（72,256 根骨骼中 4,176 根 `inheritScale`/
`inheritRotation` 为 false，未参与折叠），量级差仍达 22 倍，不影响判读。

## 3. LinkedMesh（T-C3）

Full 共 **2** 条，Primary 同为这 2 条（逐条列出）：

| 单位 | skin | slot | 附件名 | parent | parentSkin | inheritDeform | weighted |
|---|---|---|---|---|---|---|---|
| `b3240c3/b3240c3.skel.bytes` | default | body | `body_no mouth` | `body` | — | **true** | **值无效**（见下） |
| `h0164s5/h0164s5.skel.bytes` | default | body | `body_turn` | `body` | — | **false** | **值无效**（见下） |

两条都是 `default` skin 内的自引用、都指向 `body`，覆盖到 `inheritDeform` 的 true / false 两种情况。

> **2026-10-22 二次核对更正**：表中 `weighted: false` 与 `parentSkin: default` **都是旧 Reader
> 回退路径的产物，不是语料事实**。旧 Reader 解析 linked 附件时要**当场**查父附件
> （`parentSkin?.attachments[slot][parent]`），父附件在文件里排在后面且 `skin` 名为 `null`，
> 查找落空 ⇒ 走回退分支，产出 `weighted: undefined`、`uvs/vertices` 为空的对象。
> 直接读这两条 **父附件 `body`** 可知：`weighted = true`（`uvs` 114/34、`vertices` 1097/165）
> ⇒ 两条 linked 的父都是**加权 mesh**，与 `DataType_Report.md` 的「`weightedLinkedMesh` **2**」一致，
> 即类型字节 **5**（不是 4）。
> **连带纪律**：凡依赖旧 Reader 的取证，**linked 附件的继承字段**（`weighted` / `vertices` /
> `uvs` / `hull`）一律不可信，不得作为建模依据；只有**自身字段**（`parent` / `inheritDeform` /
> `path`）可信。

### 3.1 TBD-F 已关闭：类型字节直接普查（2026-10-22）

`tools/skeleton-probe/type-byte-census.mjs` 不复用旧 Reader，按 `binary_reader.cpp` 的
已验证布局独立走字节，逐个附件读 `type` 字节（类型字节是**自身字段**，不受父查找失败影响）。

| 类型字节 | 名称 | Full | Primary |
|---|---|---:|---:|
| 0 | region | 27,459 | 24,605 |
| 1 | boundingbox | 0 | 0 |
| 2 | mesh | 2,979 | 2,572 |
| 3 | weightedMesh | 17,271 | 15,751 |
| 4 | **linkedMesh** | **0** | **0** |
| 5 | **weightedLinkedMesh** | **2** | **2** |

- 走查 **Full 1,113 / Primary 963 单位，解析失败 0**；与 A1 的 region / mesh / weightedMesh /
  boundingbox 计数**逐位吻合**，可作为普查正确性的交叉验证。
- **判定：语料里没有类型 4（linkedMesh），只有 2 条类型 5（weightedLinkedMesh）**，
  且两条的父附件类型字节都是 3（weightedMesh）—— 与 `DataType_Report.md` 的
  「`weightedLinkedMesh` 2」一致，A1 原表的「都不加权」确认为回退假值。
- 影响：`MeshAttachment`（非加权）的 linked 形态**在语料中没有真实样本**；
  A2 提案的 `LinkedMeshAttachment.parentKey` 仍需同时支持指向 mesh 与 weightedMesh 两种父类型
  （结构不变），但**不得**用「语料里 linked 都是非加权」来简化实现，合成 fixture 必须覆盖类型 4。

~~补充观察：`h0164s5` 正是 B6 里共享图集布局失败的两个单位之一~~
**已更正（2026-10-22 复核）**：B6 失败的两单位是 `h0164s5/watch/h0164s5_watch_hour.skel.bytes`
与 `h0164s5_watch_minute.skel.bytes`（共用 `h0164s5_watch.atlas.txt`），二者**不含** LinkedMesh；
含 LinkedMesh 的 `h0164s5/h0164s5.skel.bytes` 自带 `h0164s5.atlas.txt`，不在失败集合里。
⇒ **LinkedMesh 与共享图集缺口没有交集**，本批样本不受 Task 6 阻塞。

## 4. `hull` / `edges` 出现率（T-C4）

| 项 | Full | Primary |
|---|---|---|
| mesh 附件（含加权、不含 linked） | 20,250 | 18,323 |
| `hull` 出现 | **20,250（100%）** | 全部 |
| `hull` == 顶点数 | 11,036（54.5%） | — |
| `hull` 其它值（0 < hull ≠ 顶点数） | 9,214（45.5%） | — |
| `edges` 出现 | **67（0.33%）** | — |
| `width` / `height`（nonessential） | 20,250（全部） | — |

- `hull` 是**必读**字段（100% 出现），取值 ≤ 顶点数，语义上是**凸包顶点数**：
  45.5% 的 mesh 的 `hull` 小于顶点数，说明它确实是凸包（不是简单的顶点数副本）。
- 可再生性：凸包可由顶点重算，但**共线点取舍、退化情况、精度**没有统一约定，
  重算结果无法保证逐字节一致。
- `edges` 近乎不出现（0.33%），且可由三角索引的边界边推导。
- 两条都支持 D31：`hull` / `edges` **不落 canonical**，导入时记为
  `IMPORT_UNSUPPORTED_CAPABILITY`（warning），与 B6 已修正的诊断码分流一致。

## 5. BoundingBox 与 path / clipping（T-C5）

- **boundingbox 附件：Full 0 / Primary 0** —— 复核确认。
- 3.2.01 的附件类型字节只有 0…5（region / boundingbox / mesh / weightedmesh /
  linkedmesh / weightedlinkedmesh）；**path 约束与 clipping 附件在该版本格式里不存在**
  （3.5+ 才引入）。所以「0 例」是**格式层面**的结论，不是抽样没遇到。
- 因此 D32 的「记录为不支持的附件类型」在 3.2.01 分支下**没有触发样本**，
  只能作为面向未来版本的诊断占位。

## 6. IK 约束

| 项 | Full | Primary |
|---|---|---|
| 数量 | 2,707 | 2,481 |
| 链长 | 1…2（mean 1.84） | 1…2 |
| 链内骨骼 / 目标骨索引越界 | 0 / 0 | 0 / 0 |
| `mix` | **只有 0（302）与 1（2405）** | 只有 0（274）与 1（2207） |
| `bendPositive` | true 1,348 / false 1,359 | true 1,232 / false 1,249 |

**新发现（需要裁定）**：`mix` 在语料里**只有 0 和 1 两个值**，没有任何中间值。
它更像**开关**（关闭 / 完全约束），不是 Spine 3.5+ 那种 0…1 插值因子。

## 7. Transform 约束

Full 208 条 / Primary 160 条，骨骼与目标骨索引越界均为 0。

| 字段（Full） | 取值构成 | 取值范围 |
|---|---|---|
| `rotateMix` | 0×164、**1×0**、其它×44（其中整数 17） | −180 … 142.6 |
| `translateMix` | 0×132、**1×0**、其它×76（其中整数 9） | −152.7 … 305.1 |
| `scaleMix` | 0×143、**1×0**、其它×65（其中整数 13） | −316.4 … 175.7 |
| `shearMix` | 全 0 | — |
| `offsetRotation` | 全 0 | — |
| `offsetX` | 全 0 | — |
| `offsetY` | 0×126、1×75、其它×7（非整数） | 0 … 1 |
| `offsetScaleX` | 0×55、1×71、其它×82（非整数） | 0 … 1 |
| `offsetScaleY` | 0×126、1×75、其它×7（非整数） | 0 … 1 |
| `offsetShearY` | 0×65、1×76、其它×67（非整数） | 0 … 1 |

**新发现（需要裁定，影响最大）**：

1. 三个 `mix` 的取值**越出 [0,1]**，且**从来没有取到 1** —— 它们不是归一化插值因子，
   而是**有量纲的量**（`rotateMix` 像角度、`translateMix` 像像素位移）。
   按 Spine 3.5+ 的「0…1 混合因子」建模会直接失真。
2. `offsetRotation` 与 `offsetX` 在语料里恒为 0；其余四个 offset 以 0/1 为主、
   少数是 (0,1) 内的小数 —— 与「mix/offset 语义互换」的直觉相反，不能照抄新版本语义。

## 8. 待裁定项（阻塞 A2，先修提案再申请批准）

| # | 争议 | 证据 | 建议（供裁定，不擅自生效） |
|---|---|---|---|
| 1 | ~~加权 mesh 顶点空间与 N2 冲突~~ → **已更正为「N2 维持」** | §2.3 对照实验：判据无效；加权各条目变换后重合（5.5 vs 122.1）、region 对照被误判、mesh 质心贴近 region 局部坐标 | A2 按**骨骼局部坐标**建模；**待负责人追认**，有异议可重跑 `tc1-space-control.mjs` |
| 2 | **负权重的合法性** | §2.2：86 个负权重，和仍 ≈1 | 导入校验**只校验权重和**，不拒绝负权重；负权重在输出里原样保留 |
| 3 | **IK `mix` 是不是开关** | §6：只取 0/1 | 先按 `bool` + 保留原始数值记录；若负责人要求按浮点建模，需说明语料无法支撑中间值的验证 |
| 4 | **Transform `mix` / `offset` 的真实语义** | §7：mix 越出 [0,1]，offset 反而是 0…1 | **不按 3.5+ 语义建模**；A2 定义为「保留原值 + 不解释语义」，等动画时基（V3）取证后再解释 |
| 5 | ~~region 计数口径差 2~~ → **已关闭** | §1：差 2 = 缺图集的 2 个单位未进入映射 | A4 需另处理 `field-mapping.md` §10.1（47,705）与 B5（47,709）、本探针（47,711）之间**另外 4 的差** |

## 9. 边界

- 本报告的解析依赖**旧项目** Reader，它是证据参照而非产品实现；产品 Reader 必须在阶段 B
  独立实现并用本报告的**统计量**对齐（不是对齐代码）。
- 骨骼世界位置是**判据级近似**（忽略 `inheritScale` / `inheritRotation`），只用于判断顶点所在空间，
  不构成求值器实现。
- 未取证：动画时间线 / deform 的时基（V3）、UV 方向（V1）—— 二者按负责人要求**先有证据再进设计**，
  不在本批范围内。
