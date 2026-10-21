# Task 5 · A1 阶段报告复核记录

- 被复核：`Agent_Task/Task5_A1_Report.md`（交付摘要）+ `docs/skeleton/mesh-constraint-evidence.md`（详细证据）
- 复核日期：2026-10-22（同日**二次核对**：A2 / A3 / A4 交付物的交叉核对见 §9）
- 复核方式：**复跑取证探针** + **交叉核对历史报告** + **对判据做对照实验** + **纪律核查**
- 结论：**有条件通过 —— 结论与数字需返修**。返修已随本记录一并落地（见 §5）。
- 未变的部分：语料规模、权重结论、`hull`/`edges`、LinkedMesh、IK、transform、boundingbox —— 全部复现通过（§4）。

---

## 1. 复核动作

| # | 动作 | 结果 |
|---|---|---|
| 1 | 在**原语料**上重跑 `mesh-constraint-probe.mjs`（Full 1113 / Primary 963） | 通过：解析失败 0；多数统计量逐位复现，但**报告有 5 处数字与复跑不一致**（§3 P2） |
| 2 | 与 B4 / B5 / B6 报告交叉核对计数 | 非 region 合计 20,252 与 B5 完全一致；region 差 2 **已定位**（§5） |
| 3 | 对 T-C1「顶点空间」判据做**对照实验**（新增 `tc1-space-control.mjs`） | **判据无效，原结论方向相反**（§3 P1） |
| 4 | 纪律核查（语料绝对路径、双分母、只读、工具登记） | 通过（§6） |

---

## 2. 复跑基线（以本次复跑为准）

```bash
node tools/skeleton-probe/mesh-constraint-probe.mjs "<语料根>" -o tools/skeleton-probe/out
node tools/skeleton-probe/tc1-space-control.mjs "<语料根>"     # 判据对照实验
```

| 项 | Full | Primary |
|---|---|---|
| 单位 / 解析失败 | 1113 / 0 | 963 / 0 |
| region / mesh / weightedMesh / linked / boundingbox | 27,459 / 2,979 / 17,271 / 2 / **0** | 24,605 / 2,572 / 15,751 / 2 / **0** |
| 加权**顶点** | **312,083** | 282,039 |
| 加权**权重条目**（每条骨骼一个） | 595,995 | 536,369 |
| 每顶点骨骼数 | 1…16，mean 1.9097 | 1…16，mean 1.9018 |
| `boneIndex` 越界 / 非整数编码 | 0 / 0 | 0 / 0 |

---

## 3. 问题清单

### P1（严重）T-C1 顶点空间的结论方向相反，判据本身无效

**报告原结论**：顶点「更接近骨架原点」⇒ 顶点是**骨架空间**的 setup 坐标 ⇒ `DECISIONS.md` N2 被推翻。

**为什么判据不成立**：把「顶点到骨骼世界位置的距离」与「顶点到原点的距离」比较时，
如果顶点真的是**骨架空间**坐标，它应当**靠近骨骼的世界位置**（网格就长在骨骼上）；
只有当顶点是**骨骼局部**坐标时，它才会靠近原点、远离骨骼世界位置。
也就是说，观测到的「离原点近、离骨骼远」恰恰是**局部坐标**的特征，不是骨架空间的特征。

**对照实验（`tc1-space-control.mjs`，n 见下）**：

| 实验 | 判据对象 | 结果 | 判读 |
|---|---|---|---|
| A2 | 加权顶点：同一顶点各骨骼条目的 (x,y) 分别变换到骨架空间后的最大两两距离 | mean **5.502**（>100 的仅 3,222/204,304） | 各条目变换后**重合** ⇒ 每条 (x,y) 是**该骨骼的局部坐标**（Spine 语义） |
| A1 | 同一顶点各条目**原始** (x,y) 的最大两两距离 | mean **122.132**（>100 的 86,358/204,304） | 原始值彼此相差很大 ⇒ 不可能是同一份骨架空间坐标（否则应重合） |
| A3 | 各条目 (x,y) 完全相同的顶点数 | 22 / 204,304 | 若存骨架空间坐标应为绝大多数，实测基本没有 |
| B | **对照**：`region.x/y`（语义已知 = 槽骨骼局部坐标）走同一判据 | 到原点 mean **50.365**、到槽骨骼世界位置 mean **377.896** | 判据把**已知局部坐标**判成了「骨架空间」⇒ **判据无效** |
| C | 同 slot 内 mesh 质心 → `region.x/y`（局部） vs → 骨架原点 | **34.766** vs 83.012 | 质心与 region 同处局部空间 |
| D | 同 slot 内 mesh 质心 → region 的**世界**位置 vs → 槽骨骼世界原点 | **435.473** vs 418.414 | 若质心是骨架空间坐标，前者应 ≈ 0；实测与「离骨骼世界位置一样远」同量级 ⇒ 局部空间 |

（A2 残差不为 0 的原因：世界变换是判据级近似，语料中 4,176/72,256 根骨骼的
`inheritScale`/`inheritRotation` 为 false 未参与折叠；量级差仍达 22 倍，不影响判读。）

**更正后的结论**：语料里的 mesh 顶点，**加权的是每条 (bone,x,y) 的骨骼局部坐标、
非加权的是槽骨骼局部坐标** —— 与 Spine 语义一致，**N2 未被推翻**。
报告 §4.6 第 1 条与证据文档 §2.3 的结论已按此更正。

### P2（严重）报告数字不可复现 / Full 与 Primary 口径混用

| 位置 | 报告原文 | 复跑值 | 判定 |
|---|---|---|---|
| §4.1 / 证据 §2.1 | 加权顶点 Full **595,995** | **312,083** | 错：595,995 是**权重条目**数（= 顶点数 × 平均骨骼数 1.91），不是顶点数 |
| §4.1 / 证据 §1 | Primary mesh **1,292**、region **24,912** | **2,572** / **24,605** | 错：报告值与其他口径不自洽（Full mesh 2,979 的 43%，而 region 占 90%） |
| §4.3 / 证据 §4 | Primary mesh 附件合计 **17,043** | **18,323** | 随上项错误连带 |
| §2.3 / 证据 §2.3 | 加权顶点到根骨骼 / 到原点 **68.40 / 68.38**（标注 Full） | Full **72.196 / 72.163**；68.40 / 68.38 是 **Primary** 值 | 错：把 Primary 值标成 Full |
| §4.1 / 证据 §2.1 | 骨骼数分布「2 骨 47.2%、1 骨 34.7%」（标注 Full） | Full 47.06% / 34.53%；47.2% / 34.7% 是 **Primary** 值 | 同上 |

其余统计量（hull / edges / IK / transform / linked / boundingbox / 权重和 / 负权重 / 越界计数）
与复跑**逐位一致**。

### P3（中）与 B6 共享图集缺口的「交集」表述不成立

报告称「`h0164s5` 正是 B6 共享图集布局失败的两个单位之一，Back Task 完成后这条 LinkedMesh 才能成为实测样本」。
语料实际布局：

```
h0164s5/h0164s5.skel.bytes            ← 自带 h0164s5.atlas.txt，不是 B6 失败单位；LinkedMesh 在这
h0164s5/watch/h0164s5_watch_hour.skel.bytes    ← B6 失败单位（共用 h0164s5_watch.atlas.txt）
h0164s5/watch/h0164s5_watch_minute.skel.bytes  ← B6 失败单位（共用同一份图集）
```

两个失败单位里**没有** LinkedMesh（语料全部 LinkedMesh 只有 2 条：`b3240c3/...` 与 `h0164s5/h0164s5.skel.bytes`）。
⇒ 「LinkedMesh 与共享图集缺口在语料中有交集」不成立，LinkedMesh 的实测样本不受 Task 6 阻塞。

### P4（小）探针输出里 LinkedMesh 的「顶点数」恒为 0

LinkedMesh 继承父附件的 `uvs`/`triangles`，自身为空 ⇒ 输出表该列恒显示 0，易被误读成「0 顶点」。
属显示问题，不影响统计（探针已不把 linked 计入顶点规模）。

### P5（小）可复现性约束

探针通过相对路径 `../../..` 借用旧项目 Reader（`converter/src/read/binary32.ts`），
仓库搬离当前位置即不可用。已在 README / `STRUCTURE.md` §2.4 注明依赖。

---

## 4. 复核通过的部分（逐位复现）

| 项 | 复跑值 | 与报告 |
|---|---|---|
| 分母与解析 | Full 1113 / Primary 963，失败 0 | ✓ |
| Full 附件分类（region / mesh / weightedMesh / linked / bb） | 27,459 / 2,979 / 17,271 / 2 / 0 | ✓（非 region 合计 20,252，与 B5 一致） |
| 权重和 = 0.99992997…1.00001000，\|sum−1\| ≤ 7.0e-5 | ✓ | ✓ |
| 负权重 86（Full）/ 80（Primary），min −0.03365 | ✓ | ✓ |
| 每顶点骨骼数 1…16；`boneIndex` 越界 0、非整数编码 0 | ✓ | ✓ |
| `hull` 出现率 100%（20,250/20,250）；= 顶点数 11,036（54.5%）/ 其它 9,214（45.5%） | ✓ | ✓ |
| `edges` 出现 67（0.33%） | ✓ | ✓ |
| LinkedMesh 2 条（skin/slot/name/parent/inheritDeform 逐条一致） | ✓ | ✓ |
| boundingbox 0 例；3.2.01 附件类型字节仅 0…5（无 path/clipping） | ✓ | ✓ |
| IK 2,707 条；链长 1…2；mix 只有 0（302）/ 1（2405），无中间值 | ✓ | ✓ |
| transform 208 条；rotateMix −180…142.6、1 从未出现；4 个 offset 落在 0…1 | ✓ | ✓ |

---

## 5. 待裁定项的变化

| 原 # | 内容 | 复核后 |
|---|---|---|
| 1 | 加权 mesh 顶点空间推翻 N2 | **结论方向更正**：证据支持「骨骼局部坐标」，N2 维持。改为待负责人**追认**此项更正（不再是需要新增取证的开放问题） |
| 2 | 负权重合法性 | 不变，仍待裁定 |
| 3 | IK `mix` 是否布尔 | 不变，仍待裁定 |
| 4 | transform `mix`/`offset` 语义 | 不变，仍待裁定 |
| 5 | region 计数口径差 2 | **已关闭**：B5 的 27,457 缺的是 2 个**缺图集**、未进入映射的单位（`h0164s5_watch_hour` / `h0164s5_watch_minute`），本探针的 27,459 = manifest 口径。**顺带发现**：`field-mapping.md` §10.1 记查找总数 47,705（4 未命中）、B5 记 47,709、本探针 47,711 —— 这 4 的差与本任务无关，A4 同步 `field-mapping.md` 时需一并处理 |

---

## 6. 纪律核查

| 项 | 结果 |
|---|---|
| 语料只读、未改产品代码 | ✓（`git status` 只新增探针/文档，未动 `importers/`、`libs/`） |
| 文档不含语料绝对路径 / 游戏资产原文（规范 §8.2 / §9.3） | ✓ |
| Full / Primary 双分母分别统计（D12） | ✓（问题只出在报告**转录**时混用，不是探针口径） |
| 新增工具登记（`STRUCTURE.md` §2.4、工具 README） | ✓（本次补登记 `tc1-space-control.mjs`） |
| 未把「可解析」表述为「可渲染/可求值」 | ✓ |

---

## 7. 返修处置（已随本记录落地）

| 文件 | 改动 |
|---|---|
| `Agent_Task/Task5_A1_Report.md` | 状态行加复核标记；§4.1 / §4.3 / §4.4 / §4.6 数字与结论按复跑更正；§5 待裁定项 1、5 更新 |
| `docs/skeleton/mesh-constraint-evidence.md` | §1（region 口径已定位）、§2.1（顶点数）、§2.3（结论方向 + 对照实验）、§3（去掉错误交集）、§8（待裁定项 1、5） |
| `tools/skeleton-probe/tc1-space-control.mjs` | **新增**：顶点空间判据的对照实验（A/B/C/D 四组），已登记 README 与 `STRUCTURE.md` §2.4 |
| `Agent_Task/Task5.md` | 状态行与 §2 T-C1 记录更正后的结论 |

## 8. 停点（2026-10-22 晚更新）

§5 的 5 项**已全部答复**（顶点空间＝「补上验证」⇒ 对照实验复跑**逐位一致**、N2 维持；
负权重 **D34**、IK 布尔 **D35**、transform 自洽 **D36**、region 口径差列 **TBD-E**）。
A2 模型提案（`docs/skeleton/mesh-and-constraint-rfc.md`）与 A3 格式 v3 提案
（`docs/format/native-project-v3-mesh.md`）**已产出、待负责人审阅**。
若负责人对 P1 的更正有异议，`tc1-space-control.mjs` 可原地重跑复核，不必重做整轮取证。

---

## 9. 附：A2 / A3 / A4 交付物的交叉核对（2026-10-22 二次核对）

| # | 发现 | 处置 |
|---|---|---|
| **F1** | 2 条 LinkedMesh 的 `weighted: false` 是**假值**：旧 Reader 解析 linked 时当场查父附件，父在文件里排后且 `skin` 名为 `null` ⇒ 走回退分支，`weighted: undefined`。实测两条的**父附件 `body` 都是加权 mesh** ⇒ 属 `weightedLinkedMesh`（类型字节 5），与 `DataType_Report.md` 的「`weightedLinkedMesh` 2」一致。**纪律**：linked 的**继承字段**（`weighted` / `vertices` / `uvs` / `hull`）一律不可信 | 证据 §3、A1 报告 §4.4、A2 §6 已更正；类型字节 4/5 直接计数由 **TBD-F** 承担 → **已关闭，见 §10** |
| **F2** | 格式 v3 提案 §2 JSON 示例里 `weightedMesh` 带 `vertices`，与同文档 §3.1 字段表（weightedMesh 的 `vertices` = 不适用）**自相矛盾** | 已删示例中的 `vertices`；A2 §5 补「不落 `vertices`，顶点数 = `vertexBindings.size()`」 |
| **F3** | 格式 v3 §6.1 第 3 步写 `kCapabilityUnsupported`，与 v2 冻结稿 §6.1（`kFormatTooNew`）**冲突** | 已按冻结稿统一为 `kFormatTooNew`，并留修订痕迹 |
| **F4** | `Task5.md` 状态行、`Task5_A1_Report.md` §2/§7 仍写「A2–A5 待裁定后继续 / 后续 A3→A4→A5」，但实际 A2、A3 已产出、A4 部分完成 | 状态行与清单已同步为逐步骤状态表 |
| **F5** | 新文档 `mesh-and-constraint-rfc.md`、`native-project-v3-mesh.md` **未登记** `STRUCTURE.md` | 已补登记（§文档表 + §文档索引） |
| **F6** | `field-mapping.md` / `skeleton-domain-rfc.md` 的「mesh 2,981」是 B4 旧口径（= 2,979 + linked 2），与新的 linked 单列**重复计数** | 已改为 2,979 并注明旧口径换算 |
| **F7** | `DataType_Report.md` 待实现清单仍写「后续任务 + **独立 capability**」，与 **D29**（升 v3、不做 capability）**冲突** | **已改（2026-10-22）**：§3「去向」列按 D29–D33 重写，类型计数按普查同步；抬头标注为「Task 4 历史快照，盘查事实未改」 |
| **F8** | A1 报告 §4.5 把「path/clipping 是 3.5+ 引入」当作语料证据 | 已标注为**外部知识**，格式层面结论只依赖旧 Reader 的 3.2.01 分支定义 |
| **F9** | A1 报告 §3 记「耗时 8.5 s」，复跑实测 12.2 s；且未列对照实验命令 | 已补两个耗时与第二条命令 |

二次核对中**逐位复现**的部分：对照实验 A1/A2/A3/B1/B2/C1/C2/D1/D2 全部与首轮一致
（`tc1-space-control.mjs` 输出确定，可安全重跑）。

---

## 10. 附：TBD-F 关闭（2026-10-22 三次核对，类型字节直接普查）

负责人要求补做类型字节直接计数，并修正 `DataType_Report.md`（F7）。结果：

**新增工具** `tools/skeleton-probe/type-byte-census.mjs` —— **不复用旧 TS Reader**，
按 `importers/spine32/src/binary_reader.cpp`（B4 已验证的字节走查结论）独立遍历
`header → bones → ik → transform → slots → skins`，逐个附件读 `type` 字节后按类型专属布局跳过载荷。
只走到 skins 段为止（**不解析 events / animations**），因此**不做 EOF 断言**；
它是普查器不是校验器，正确性靠与 A1 已知计数的交叉核对兜底。

| 类型字节 | 名称 | Full | Primary | 与 A1 交叉核对 |
|---|---|---:|---:|---|
| 0 | region | 27,459 | 24,605 | ✅ 逐位一致 |
| 1 | boundingbox | 0 | 0 | ✅ |
| 2 | mesh | 2,979 | 2,572 | ✅ |
| 3 | weightedMesh | 17,271 | 15,751 | ✅ |
| 4 | **linkedMesh** | **0** | **0** | —（本次新增结论） |
| 5 | **weightedLinkedMesh** | **2** | **2** | ✅ 合计 2/2 |

走查 **Full 1,113 / Primary 963 单位，解析失败 0**，耗时约 1.9 s。

**结论**：语料里**没有**类型 4（linkedMesh），2 条都是类型 5（weightedLinkedMesh），
且两条的父附件类型字节均为 3（weightedMesh）。F1 的推断被直接证据坐实，
A1 原表「两条都不加权」确认为旧 Reader 回退分支的假值。
⇒ **非加权 linked 无真实样本**，A5 合成 fixture 必须覆盖类型 4。

实施过程中的两条自建教训（已写进工具注释，避免重犯）：
1. **字节序**：Spine 二进制的多字节数值是**大端**（C++ `i32` / `f32` 均按大端拼接）。
   首版写成 LE，走查在第一段浮点处即错位，全语料 1086/1113 失败。
2. **上限护栏**：`count()` 必须带 `kMaxArrayCount`（1<<20）等上限。首版缺护栏，
   错位时会读到巨大 count 并空转到超时（首次运行被 SIGTERM 杀掉）。

**改动文件**：新增 `type-byte-census.mjs`（已登记 README 与 `STRUCTURE.md` §2.4）；
`mesh-constraint-evidence.md` 新增 §3.1；`Task5_A1_Report.md` §4.4 补普查表；
`mesh-and-constraint-rfc.md` §6 与 TBD-F 行改为已关闭；`DataType_Report.md` §3（F7）。
