# Task 5 · A1 第二批静态数据语料取证 — 交付报告

- 任务：`Agent_Task/Task5.md` §2（T-C1…T-C5）、§3 A1
- 日期：2026-10-22
- 状态：**A1 完成并通过复核**（2026-10-22）；5 项待裁定**已全部答复**（见 §5）；**A2 RFC 已产出** → `docs/skeleton/mesh-and-constraint-rfc.md`
- **2026-10-22 复核**：数字与 T-C1 结论**已返修**（Full/Primary 口径混用 5 处、顶点空间结论方向相反），
  复核记录见 `Agent_Task/Task5_A1_Review.md`；本报告数字以复核后为准
- 详细证据：`docs/skeleton/mesh-constraint-evidence.md`（本报告只做交付摘要，不重复全文）

## 1. 范围与纪律

| 项 | 内容 |
|---|---|
| 范围 | 第二批静态数据（mesh / 加权 mesh / linked mesh / IK / transform）的**分布与边界**取证；boundingbox 与 path/clipping 的 0 例复核；非有限浮点扫描 |
| 纪律 | **只读**语料；不写产品代码；不修改/复制任何游戏资产 |
| 分母 | **Full 1113** 与 **Primary 963** 双口径（D12），分别统计，不合并 |
| 解析依据 | 旧项目 `converter/src/read/binary32.ts`（**证据参照**）；产品 Reader 在阶段 B 独立实现，不以本探针为依据 |
| 输出 | 只有统计量与**相对路径**；不含盘符/用户名，不含语料原文（规范 §8.2 / §9.3） |

## 2. 交付物清单

| 文件 | 说明 |
|---|---|
| `tools/skeleton-probe/mesh-constraint-probe.mjs` | 新增只读探针（零第三方依赖，Node v22+） |
| `docs/skeleton/mesh-constraint-evidence.md` | A1 证据报告（含 §8 待裁定项） |
| `tools/skeleton-probe/out/mesh-constraint-probe.json` / `.md` | 机器可读 / 人读结果（生成目录，可删可重建） |
| `tools/skeleton-probe/tc1-space-control.mjs` | **判据对照实验**（复核时新增）：用已知答案样本检验顶点空间判据本身 |
| `Agent_Task/Task5_A1_Review.md` | A1 复核记录（数字返修与 T-C1 结论更正） |
| `docs/skeleton/mesh-and-constraint-rfc.md` | **A2** canonical 模型提案（承接本节结论） |
| `docs/format/native-project-v3-mesh.md` | **A3** 原生格式 v3 提案（承接 A2） |
| `tools/skeleton-probe/README.md` | 工具登记（用途表、运行命令、输出） |
| `STRUCTURE.md` §2.4 | 工具登记簿同步 |
| `Agent_Task/Task5.md` | 状态行同步（A1 完成并通过复核） |

## 3. 验证

```bash
# 语料根由负责人提供，不写进文档（规范 §8.2）
node tools/skeleton-probe/mesh-constraint-probe.mjs "<语料根>" -o tools/skeleton-probe/out

# 判据对照实验（必须和取证一起跑，见 §4.6 第 1 条）
node tools/skeleton-probe/tc1-space-control.mjs "<语料根>"
```

- 扫描 **Full 1113** 个单位、**Primary 963** 个单位，**解析失败 0**，耗时 8.5 s（首轮）/ 12.2 s（复核复跑）。
- 语料根给错时探针以**退出码 3** 拒绝出报告（沿用 smoke 的 0 匹配守门纪律）。
- 可安全重跑：按路径排序 + 纯聚合，输出确定。
- **复跑验证（2026-10-22，负责人要求「补上验证」）**：在原语料重跑
  `node tools/skeleton-probe/tc1-space-control.mjs "<语料根>"`，单位 1113、骨骼 72,256
  （`inheritScale`/`inheritRotation` 为 false 的 4,176），A1 **122.132** / A2 **5.502** /
  A3 **22/204,304** / B1 **50.365** / B2 **377.896** / C1 **34.766** / C2 **83.012** /
  D1 **435.473** / D2 **418.414** —— 与首次运行**逐位一致**，顶点空间结论可复现。

## 4. 关键证据摘要

### 4.1 规模与分类（Full / Primary）

| 项 | Full | Primary |
|---|---|---|
| region / mesh / weightedMesh / linked / boundingbox | 27,459 / 2,979 / 17,271 / 2 / **0** | 24,605 / 2,572 / 15,751 / 2 / **0** |
| 加权**顶点** | **312,083** | 282,039 |
| 加权**权重条目**（每条骨骼一个） | 595,995 | 536,369 |
| 每顶点骨骼数 | 1…16，mean 1.9097（2 骨 47.06%） | 1…16，mean 1.9018（2 骨 47.2%） |
| `boneIndex` 越界 / 非整数编码 | **0 / 0** | **0 / 0** |

- 与 B4 / B5 对账：非 region 合计 **20,252** 与 B5 的「非 region 跳过」精确一致；
  B4 的 mesh 分组 **2,981** = 本探针 mesh **2,979** + linked **2**（B4 把 linked 计入 mesh）。
  加权顶点 Full **312,083**（不是权重条目数 595,995 —— 复核已更正，见 `Task5_A1_Review.md` §3 P2）。
- ~~口径待核：region 27,459 vs B5 的 27,457~~ **已定位（复核）**：B5 缺的 2 个是**缺图集、未进入映射**
  的单位（`h0164s5_watch_hour` / `h0164s5_watch_minute`），27,459 才是 manifest 全量口径。

### 4.2 T-C2 权重

- 权重和 0.99993…1.00001，**|sum−1| ≤ 7.0e-5**（float32 舍入量级）→ 语料已归一化，
  D30「保留原值 + 导入期校验」成立，容差取 **1e-3** 足够宽松。
- 负权重 **86**（Full）/ 80（Primary），min −0.03365，且这些顶点和仍 ≈1。

### 4.3 T-C4 `hull` / `edges`

- `hull` 出现率 **100%**（Full 20,250/20,250；Primary 18,323），但 **45.5%** 的 `hull` ≠ 顶点数 → 确为凸包顶点数，
  重算无法保证一致（共线点/退化/精度无统一约定）。
- `edges` 出现率 **0.33%**（67/20,250）。
- 两条都支持 D31：不落 canonical，导入记为 `IMPORT_UNSUPPORTED_CAPABILITY`（warning）。

### 4.4 T-C3 LinkedMesh（Full 恰好 2 条）

| 单位 | skin | slot | 附件名 | parent | inheritDeform | weighted |
|---|---|---|---|---|---|---|
| `b3240c3/b3240c3.skel.bytes` | default | body | `body_no mouth` | `body` | **true** | **值无效**（见下） |
| `h0164s5/h0164s5.skel.bytes` | default | body | `body_turn` | `body` | **false** | **值无效**（见下） |

> **2026-10-22 二次核对更正**：`weighted` 列的 `false` 是**旧 Reader 回退路径**的假值
> （父附件解析落空 ⇒ `weighted: undefined`）。直接读父附件 `body` 可知两条的父都是**加权 mesh**
> ⇒ 属 `weightedLinkedMesh`（类型字节 **5**），与 `DataType_Report.md` 的「`weightedLinkedMesh` 2」一致。
> **linked 的继承字段不得作为建模依据**（证据 §3 同款标注）。

**TBD-F 已关闭（2026-10-22，类型字节直接普查）** —— `tools/skeleton-probe/type-byte-census.mjs`
不复用旧 Reader，按 `binary_reader.cpp` 已验证布局独立走字节（Full 1,113 / Primary 963，
**解析失败 0**，region 27,459/24,605、mesh 2,979/2,572、weightedMesh 17,271/15,751 与 A1 **逐位吻合**）：

| 类型字节 | 名称 | Full | Primary |
|---|---|---:|---:|
| 4 | linkedMesh | **0** | **0** |
| 5 | weightedLinkedMesh | **2** | **2** |

⇒ 语料**没有**类型 4，2 条都是类型 5，且父附件类型字节均为 3（weightedMesh）。
影响：**非加权 linked（类型 4）在语料中没有真实样本**，合成 fixture 必须覆盖它。

~~`h0164s5` 同时是 B6 共享图集布局失败的单位之一~~ **已更正（复核）**：B6 失败的两单位是
`h0164s5/watch/h0164s5_watch_hour|minute.skel.bytes`（共用 `h0164s5_watch.atlas.txt`），
二者**不含** LinkedMesh；含 LinkedMesh 的 `h0164s5/h0164s5.skel.bytes` 自带图集，**不受 Task 6 阻塞**。

### 4.5 T-C5 BoundingBox / path / clipping

- boundingbox **0 例**（Full 与 Primary 双口径复核）。
- 3.2.01 附件类型字节只有 0…5（旧 Reader 的 `ATTACHMENT_TYPE` 表），**该分支不解析 path 约束与 clipping 附件**
  → 「0 例」是**格式层面**结论，不是抽样没遇到；D32 在 3.2.01 分支下无触发样本。
  （**来源标注**：「path/clipping 是 3.5+ 引入」属 Spine 版本史的**外部知识**，不在语料证据内；
  本结论只依赖「旧 Reader 的 3.2.01 分支没有这两类」。）

### 4.6 三个推翻既有假设的发现

1. ~~顶点空间推翻 N2（T-C1）~~ **结论方向已更正（复核）**：原判据（顶点到骨骼世界位置的距离 vs
   到骨架原点的距离）本身无效 —— 若顶点是骨架空间坐标，它应当**靠近骨骼的世界位置**，
   而「离原点近、离骨骼远」恰恰是**骨骼局部坐标**的特征。对照实验已证实：
   `region.x/y`（语义已知为槽骨骼局部）用同一判据也会被判成「骨架空间」（到原点 mean 50.4、
   到槽骨骼世界 mean 377.9）；加权顶点各骨骼条目变换到骨架空间后**重合**（mean 5.5，
   原始值 mean 122.1）⇒ 每条 (x,y) 是**该骨骼的局部坐标**；同 slot 的 mesh 质心靠近
   `region.x/y`（34.8）而远离其世界位置（435.5）。**结论：N2 维持，按骨骼局部坐标建模。**
   对照实验可原地重跑：`tools/skeleton-probe/tc1-space-control.mjs`。
2. **Transform 的 mix 不是 0…1 因子**：`rotateMix` ∈ [−180, 142.6]、`translateMix` ∈ [−152.7, 305.1]、
   `scaleMix` ∈ [−316.4, 175.7]；三者**从未取到 1**，只有 0 或其它值；反倒是 4 个 offset 落在 0…1。
   按 Spine 3.5+ 语义建模会失真。
3. **IK 的 `mix` 只有 0/1**（302 个 0、2405 个 1），无中间值 → 更像开关而非插值因子。

## 5. 裁定结果（2026-10-22 负责人答复）

| # | 争议 | 负责人答复 | 落地位置 |
|---|---|---|---|
| 1 | 加权 mesh 顶点空间（N2） | **补上验证** | 已复跑对照实验（§3，逐位一致）⇒ **N2 维持**；A2 按**骨骼局部坐标**建模 —— 加权：每条 `(bone,x,y)` 属对应骨骼局部；非加权：槽骨骼局部。`mesh-and-constraint-rfc.md` §4 / §5 |
| 2 | 负权重合法性 | **合法，意义是权重无影响** | `DECISIONS.md` **D34**；导入校验**只校验权重和**，负值不拒绝、不诊断，原值保留 |
| 3 | IK `mix` 是否布尔 | **添加布尔记录是否开启** | `DECISIONS.md` **D35**：新增 `enabled: bool`（`mix != 0`），同时保留 `mix` 原值 |
| 4 | Transform mix / offset 语义 | **用自洽方案，最符合目标即可** | `DECISIONS.md` **D36**：保留原值、**不解释语义**；求值器在 V3 取证完成前不得读取这些字段做计算 |
| 5 | region 计数口径差 | **后续解决：先调查外加核对** | 差 2 已定位（缺图集的 2 个单位未进入映射）；`field-mapping.md` §10.1（47,705）/ B5（47,709）/ 本探针（47,711）之间**另外 4 的差**列为后续项 TBD-E |

## 6. 已知限制

- 骨骼世界位置是**判据级近似**（忽略 3.2 的 `inheritScale` / `inheritRotation` 折叠语义），
  只用于判断顶点所在空间，不构成求值器实现。
- 解析依赖旧项目 Reader；产品 Reader 必须在阶段 B 独立实现，并用本报告的**统计量**对齐（不是对齐代码）。
- 未取证：动画时间线 / deform 的时基（V3）、UV 方向（V1）—— 按负责人要求**先有证据再进设计**，不在本批范围。

## 7. 停点

阶段 A 进度（2026-10-22 二次核对后）：

| 步骤 | 状态 |
|---|---|
| A1 取证 | **完成并通过复核**（本报告 + `Task5_A1_Review.md`） |
| A2 canonical 模型 RFC | **已产出** `docs/skeleton/mesh-and-constraint-rfc.md`（待审阅） |
| A3 原生格式 v3 提案 | **已产出** `docs/format/native-project-v3-mesh.md`（待审阅） |
| A4 支持矩阵与诊断同步 | **部分完成**：`field-mapping.md` §11 与 `skeleton-domain-rfc.md` §10.1/§10.2 已同步；`DataType_Report.md` 待实现清单、`DECISIONS.md` 待验证表（T-C3…T-C5）**未同步** |
| A5 合成 fixture（≥4） | **未开始** |

阶段 B 实现在 A 阶段获批前仍禁止（Task 5 §3 A6）。
