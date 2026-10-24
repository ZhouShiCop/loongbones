# 类型覆盖台账：尚未实现的类型、排期与前置

- 日期：2026-10-22（含 B1/B2 复核结论同步）
- 性质：**只读盘点 + 排期视图**，不改契约、不替代任何 Task 文档
- 依据：`Agent_Task/DataType_Report.md` §3（Task 4 阶段 B 收尾时的类型盘查）、
  `docs/skeleton/field-mapping.md` §10–§11、`docs/skeleton/skeleton-domain-rfc.md` §10、
  `Agent_Task/Task5_Implementation_Plan.md`、`Agent_Task/Task5_B1B2_Review.md`、`Agent_Task/Task6.md`、`DECISIONS.md` D29–D38
- 语料口径：Full **1113** / Primary **963** 双口径（D12），本表除注明外均为 **Full 1113**；
  类型计数来自 `tools/skeleton-probe/type-byte-census.mjs`（直接读类型字节，不经旧 Reader 的继承回退）
- 基线提交：`31c87f6`（B6 遗留 + 阶段 A 交付）、`049b1d3`（阶段 B 批准开工状态）

---

## 1. 已实现的边界（对照用）

| 层 | 已落地 |
|---|---|
| Domain 骨架首批 | `Skeleton` / `Bone` / `Slot` / `Skin` / `SkinEntry` / `RegionAttachment` / `SourceRef` / `ColorF` / `BlendMode` |
| Domain 图集 | `TextureAsset` / `AtlasPage` / `AtlasRegion` / `Atlas` / `RegionRotation` |
| Domain 场景 | `Project` / `Scene` / `GroupNode` / `ImageNode` / `SpriteNode` |
| 原生格式 | v1 codec（Task 3）、**v2** skeleton 增量（Task 4） |
| 导入 | `importers/api` 契约 + `importers/host` 事务 + `importers/spine32`（region 与静态结构） |

**交付边界**：「结构可读」—— 可声称解析 / 建模 / 持久化 / 重开一致；**不得**声称可渲染 / 可蒙皮 / 可播放 / 可求值。

---

## 2. 尚未实现的类型与能力（台账）

### 2.1 第二批静态数据 —— **进行中**（Task 5 阶段 B）

> **截至 2026-10-22 晚的状态**：
> - **B1+B2 已验收通过（二次复核）**：首轮 P0（v3 保存后重复键）已返修，二次复核验证多轮往返无重复键、core 135 / format 86、零 warning、ctest 7/7；**解锁 B3**。详见 `Agent_Task/Task5_B1B2_Review.md` §7。
> - **B3 已验收通过（二次复核，2026-10-22 晚）**：首轮两个 P0（16 fixture 未接入 / 断言下限未达）已返修闭合；二次复核独立复现干净构建 0 warning、ctest 7/7、binary 47/47、mapper 48/48、core 135、format 86；并以 option=ON 对真实语料 1113 单位独立复跑端到端闭环（region 未命中 0、8/8 提交重开一致、第二批能力从 unsupported 账本消失）。详见 `Agent_Task/Task5_B3_Review.md` §9。
> - **B4 复核首轮不通过（2026-10-22）**：产品代码完成、回归全绿，但 P0-1 新增断言仅 +1（计划下限 ≥10）、P0-2 插件层真实 fixture 端到端未做（第二批进 canonical / 账本无第二批条目 / 拒绝路径经插件报出域码 均无最新证据）；观察项 smoke 未在 build_b4 重跑、mingw 退出码 2 未查明。详见 `Agent_Task/Task5_B4_Review.md`。
> - **B4 二次复核仍不通过（2026-10-23）**：P0-1 ✅（新增 12 ≥10）、P1 ✅（smoke 重跑 exit 7 = 已知缺口）、P2 ✅（make 退出码 0，根因=首次配置未启用 smoke 目标 + 缓存不完整，非编译错误）；**P0-2 的 4 个拒绝路径 09/10/12/13 未闭合**（插件层 `draft.valid == true`、adoptData 放行），测试保留为红，**ctest 6/7** ⇒ **不授权 B5**。复核关键发现：mapper 层 `domainRejectCode()` 用空 `NameRegions({})` + 仅骨架 candidate，与插件层非同口径，不能据此排除 Domain；推论为「附件未进 candidate」，定位方向应从「Domain 为何放行」改为「映射是否产出附件」。详见 `Agent_Task/Task5_B4_Review.md` §9。
> - **B4 三次复核通过（2026-10-23）**：根因是**真实产品缺陷** —— `Status::ok()` 为**静态工厂**（`diagnostics.hpp:129`，判成功须用 `isOk()`），`!status.ok()` 恒为 false ⇒ 插件 Domain 校验闸门是死代码、静默吞掉全部校验失败；改为 `isOk()` 后 09/10/12/13 四项由红转绿。独立复跑：cmake/make exit 0 无 warning、ctest **7/7**、skeleton_import **40/40**、smoke 1111/1113 + 8/8 重开一致 + exit 7（已知共享图集缺口，未包装成通过）。**第二批能力确认不在 unsupported 账本**。**不自动授权 B5**。**该同类缺陷已修（2026-10-23 复核）**：`atlas_import.cpp:318` 改为 `isOk()`；全仓无残留 `.ok()` 代码误用；重编 exit 0 无 warning、ctest 7/7、atlas_import_tests 仍绿 ⇒ 闸门由死变活未破坏既有导入。**残留已闭合（2026-10-23 晚）**：新增 `RegionExceedingPageIsRejectedByDomain`（区域越出页边界 → `kDomainRejected` + 消息含「矩形越出页边界」+ 不落盘），atlas 测试 **8 → 9**；**反向探针**：临时还原成 `ok()` 后该测试立即失败（8/9、exit 1）⇒ 断言可被证伪，确为闸门护栏；已还原并重编复跑通过（ctest 7/7）。详见 `Agent_Task/Task5_B4_Review.md` §10.8。
> - **B5 已验收通过（2026-10-24，本对话独立复跑）**：干净目录 `build_b5` ⇒ make exit 0 无 warning、ctest **7/7 exit 0**；双口径 **Full 1113/1113、Primary 963/963**（`binary-read-smoke` 退出码 **0**，manifest 16/16）；逐项语义比对 `[deep-compare] items=18067 maxResidual=0 mismatches=0`（8 个真实样本、含第二批字段，native double 往返精确）；完整提交 8/8 重开一致、region 未命中 0。`skeleton-import-smoke` 退出码 **7** = 共享图集已知缺口（Task 6），**不作验收证据**。工具 README 已登记，**根 `STRUCTURE.md` §2.12 由本对话补登记**。阶段 B（B1–B5）至此全部完成并通过复核。详见 `Agent_Task/Task5_B5_Review.md`。
> - **Task 5 已于 2026-10-24 全部完成并归档**（`Agent_Task/Task5_Completion_Report.md`）：阶段 A + B1–B5 全部通过复核；Task 5 此后不再新增改动。遗留项交接：共享图集 2 个单位 → **Task 6**（阶段 A 方案 `Agent_Task/Task6_Implementation_Plan.md` 已产出、**待批准**）；BoundingBox → D32 / Qt GUI 阶段；动画 → D39 / Phase 3。本台账继续作为「还有哪些类型没实现」的唯一入口维护。
> - **路线（负责人 2026-10-24）**：**Task 6 是小体量**收尾（共享图集布局，2 个单位）；**Task 7 起为「完全实现所有功能、至少对齐 Spine」**（**已立项 2026-10-24**，阶段 A 方案 `Task7_Implementation_Plan.md`），量级远大于 Task 5，**单独立项 + 阶段 A 取证**，不得并入 Task 6。范围见 `Agent_Task/Task7.md`，**阶段 A 可开工**。
> 下表「当前状态」列指**能力状态**（已落地、已验收）。**V3 时基已定案 D39（默认 60 FPS）、Q6-1 已定案 D40（共享图集复用一份资产）；B3 验收通过，但按阶段 B 纪律不自动授权 B4/B5**（B4 开工前本对话先验收 B4）。

| 类型 / 能力 | 语料规模（Full） | 计划归属 | 当前状态 | 前置 |
|---|---:|---|---|---|
| `WeightedMesh` + 权重绑定（顶点 312,083） | **17,271** | Task 5 B1–B5 | **类型已落地（reader/mapper 解析 + fixture golden 断言 + 语料闭环）** | B1/B2 二次复核 ✅、B3 二次复核 ✅、阶段 A 验收 ✅ |
| `MeshAttachment`（非加权） | **2,979** | 同上 | 同上 | 同上 |
| `weightedLinkedMesh`（类型字节 5） | **2** | 同上 | 同上（fixture `14` 已对齐语料；D38 偏离经 08/14 验证合理） | 同上 |
| `linkedMesh`（类型字节 4） | **0**（语料无样本） | 同上 | 同上（合成 fixture `08` 已覆盖） | 同上 |
| `IkConstraint` | **2,707** | 同上 | 同上；`enabled = (mix != 0)`（D35）、`bendPositive` 已修正为 `0xFF`=false | 同上 |
| `TransformConstraint` | **208** | 同上（**只承载原值、不解释语义**） | 同上；语义解释待真实样本（D36；V3 时基已由 D39 定 60 FPS，但 mix/offset 语义仍不解释） | 同上 |
| `BoundingBoxAttachment` | **0**（语料无样本） | **D49 裁定（2026-10-24）：纳入 Task 7**（**取代 D32**「延后至 Qt GUI」） | 类型占位与诊断码已有；Task 7 按**非权重口径**补齐解析 / 映射 / 校验 / 导入 / v4 落盘 | **Task 7 立项 + T7-D**；⚠️ 语料 0 例 ⇒ 即便实现也只可写「**已实现 + 待真实样本验证**」，**不得**写「已验证支持」（D13） |
| `hull` / `edges` | 出现率 67 / 20,250 | **D31 裁定：不落库** | 计入 unsupported 账本 | — |
| `path` / `clipping` 附件 | 3.2.01 无此分支（3.5+ 引入） | 不建模 | — | — |

### 2.2 导入能力缺口（非类型，但影响可导入单位数）

| 项 | 规模 | 归属 | 状态 | 前置 |
|---|---:|---|---|---|
| **共享图集布局**（多个骨架共用**同一份** `.atlas.txt`；真实形态为 `<上级目录名>_<目录名>.atlas.txt`，**非**"与目录同名"） | 2 个单位（`h0164s5/watch/`），**语料仅此一处** | **Task 6**（D33 单独立项） | ✅ **阶段 A / B 均通过（2026-10-24）**：`1113/1113 prepare 成功、失败 0、smoke 退出码 0`；**已知限制（D55）**：两共享单位同工程导入**各建一份 Atlas/Texture**（同一 PNG 两份字节） | **D40 / D53 / D54 已定**；**D53 落地 → Task 7 / v4**（需源身份字段） |

### 2.3 尚未排期（Phase 3 及以后）

| 类型 / 能力 | 语料规模 | 归属 | 状态 | 前置（硬阻塞） |
|---|---:|---|---|---|
| **动画 / 全部时间线**（含 deform、事件） | `timelines` **4,988,424**<br>`deformTimelines` 335,817<br>`animations` 16,007<br>`events` 1 | Phase 3（**Task 7**，**已立项 2026-10-24**；阶段 A 方案 `Task7_Implementation_Plan.md`） | 未开工 | **V3 时基已由 D39 定案（默认 60 FPS）** ⇒ 原「时基 / 帧率未决」硬阻塞**已解除**（3.2.01 二进制头无 fps 字段、禁止写死 30 的约束仍有效）；剩余前置 = Phase 2 收尾（**Task 6** 共享图集）+ Task 7 立项及其阶段 A 取证。**D44 完全扩展**（连续 / 曲线 bezier 含控制手柄 / 额外手柄控制函数的复杂行为，不只做原值承载）；**D46 原生格式升 v4**；**D47 先定清「时间平滑程度」等基础问题**（时间粒度 / 采样 / 插值基准与 D39 整数 tick 的衔接）**再铺开** |
| Deform 时间线与网格顶点编辑 | 335,817 | Phase 4 | 未排期 | 稳定顶点 ID / 索引契约 + Phase 3 Runtime |
| 蒙皮求值 / IK 求解 / 权重归一化落地 | —（17,271 加权附件的求值是消费者） | Phase 4（**部分提前到 Task 7**） | 未排期；**D45 已裁定拆分**：**非权重部分（骨骼变换层级 / 子物体矩阵）可在 Task 7 做**，**权重显示等权重相关延后** | Phase 3 Runtime 存在；D30 把归一化推迟到求值侧 |
| `SkeletonNode`（场景中的骨架实例） | — | 另立任务（**D27**） | 未排期 | 会改 v1 `SceneNode::type` 枚举 ⇒ 需独立任务 + 迁移方案 |
| 多皮肤换装 / 皮肤混合 | 语料无真实样本（T-B4） | 后续任务 | 未排期 | 真实样本（当前 0 例，不得用合成样本宣称兼容） |
| 外部引用纹理的导入产出 | — | 设计决定 | 不实现 | 保留 `external` 枚举供用户显式使用，**导入器不产出** |
| DragonBones | — | 规范 §1 **已放弃** | 不做 | — |
| GUI / 渲染 / 视口 | — | 另行排期 | 未排期 | Qt 技术切片 |

### 2.4 已实现但语义未验证（不得当"支持"用）

`rotation≠0` 与页缩放复合（T-A1）、继承布尔分布（T-A2）、`pma:true`（T-B1）、
`split`/`pad`（T-B2）、多皮肤 / 多页图集（T-B4）、`deform offset` 形态（T-B5）。
清单与责任阶段见 `docs/skeleton/skeleton-domain-rfc.md` §11.1；
T-B3（尾随空格降级匹配）已由语料实测 0 例降级 + B6 测试覆盖，**可结**。

---

## 3. 排期与量级

### 3.1 时间口径（先说清楚，避免虚假日历承诺）

- 参照基准：Task 4 阶段 B 的 **B0–B6 共 7 个工作包 ≈ 2 个日历日**（含取证与一轮返修）；
  **全新全量编译 ≈ 20 分钟**，增量 **1–3 分钟/次**。
- 因此下表只给 **实现会话数 + 代码行量级 + 编译等待**，不给日历日期。
  日历时间取决于会话并行度与返修轮次（历史上一轮返修 ≈ 0.5 个日历日）。

### 3.2 量级表

| 项 | 工作包 | 产品代码 | 测试代码 | 实现会话 | 编译等待 |
|---|---|---:|---:|---:|---:|
| **Task 5 阶段 B**（第二批静态） | B1 Domain / B2 格式 v3 / B3 Reader+Mapper / B4 插件 / B5 语料闭环 | 1,400–2,000 行 | 1,400–1,800 行 | **5–7**（B3 独占 2–3） | 1.5–3 小时 |
| — B1/B2 **返修**（P0 + 补断言） | `decode.cpp` 三处已知键 / 未知字段收集 + core ≥15 / format ≥15 断言 | 30–60 行 | 250–400 行 | **1** | 1–3 分钟/次 |
| — B3（最大） | `binary_reader.cpp`（+246，mesh 族与约束真实解析）+ `mapper.cpp`（+180，第二批映射）+ `source_model.hpp`（+84） | 600–900 行 | 600–900 行 | 2–3 | — |
| — B3 **返修**（fixture golden + 断言下限） | 16 fixture 接入产品测试逐条断言 golden（含 08/14 linked 两路径、09/10/12/13 拒绝、11/15 约束）+ binary ≥20 / mapper ≥15 断言 | 30–60 行 | 250–400 行 | **1** | 1–3 分钟/次 |
| **Task 6**（共享图集布局） | `IResourceResolver` 目录列举 + 宿主契约变更 + B6 回归 | 150–300 行 | 150–250 行 | **1–2** | 0.5–1 小时 |
| **时基取证**（V3 未决的解锁动作） | 从真实样本推 fps / 舍入规则 | 探针 1 个 | — | **1–2** | — |
| **动画建模**（Task 7，未立项） | 时间线族（translate/rotate/scale/shear/attachment/color/deform/drawOrder/event/ik/transform） | ≈ Task 5 的 **2–3 倍** | 同 | **10–15** | 数小时 |
| Phase 4（蒙皮 / IK 求值 / deform 编辑） | 依赖 Runtime | 未知 | 未知 | 未估 | — |

### 3.3 顺序

```text
[进行中] Task 5 阶段 B：B1/B2 二次复核 ✅ ──► B3 二次复核 ✅ ──► B4 三次复核 ✅（含 atlas 同类缺陷已修 + 拒绝断言已补并反向验证）──► **B5 进行中**（smoke 目标待补 `#include <cmath>`）──► 验收（本对话）
[并行可做] Task 6（需 Q6-1 先裁定）
         │
         ▼
[解锁动作] V3 时基取证 ──► [未立项] 动画建模（Task 7）──► Runtime ──► [Phase 4] 蒙皮 / IK / deform
```

Task 5 与 Task 6 **互不阻塞**（Task 6 会触及 B6 的 10 项宿主事务测试，宜在 B6 断言仍新鲜的窗口内做，
或明确安排回归；两者同时改 `importers/` 时需避免同一工作区交错）。
动画被 **V3 时基** 硬阻塞 —— 时基不定，时间线无法采样，任何"先写死 30 FPS 再改"的做法被 D3 / V3 明确禁止。

---

## 4. 前置条件汇总

### 4.1 已满足（Task 5 阶段 B）

- 阶段 A 六项验收通过（`Agent_Task/Task5_A6_Acceptance.md`）
- R1 B6 遗留改动已入基线（`31c87f6`）
- 阶段 B 方案已批准（`Agent_Task/Task5_Implementation_Plan.md` §9）
- 契约基线冻结：D29–D38、模型提案 `mesh-and-constraint-rfc.md`、格式提案 `native-project-v3-mesh.md`、
  16 个 fixture 的 golden 表
- **B1 / B2 已验收通过（二次复核，2026-10-22 晚）**：首轮不通过（P0 重复键），返修已完成并经二次复核验证（干净构建 0 warning、ctest 7/7、core 135 / format 86、多轮往返无重复键断言 PASS），验收解除、**解锁 B3**；见 `Agent_Task/Task5_B1B2_Review.md` §7
- **B3 已验收通过（二次复核，2026-10-22 晚）**：首轮两个 P0（16 fixture 未接入 / 断言下限未达）已返修闭合；二次复核独立复现干净构建 0 warning、ctest 7/7、binary 47/47、mapper 48/48、core 135、format 86；并以 `-DLOONGBONES_BUILD_SKELETON_IMPORT_SMOKE=ON` 对真实语料 1113 单位独立复跑端到端闭环（region 未命中 0、8/8 提交重开一致、第二批能力从 unsupported 账本消失、`IMPORT_UNSUPPORTED_CAPABILITY` = 6636）。见 `Agent_Task/Task5_B3_Review.md` §9。
  —— **按阶段 B 纪律不自动授权 B4/B5**：B4 开工前本对话先验收 B4。

### 4.2 硬阻塞（未满足即不能开工）

| 阻塞项 | 卡住谁 | 解除条件 |
|---|---|---|
| ~~B1/B2 返修二次复核（P0 重复键 + 补断言）~~ | ~~B3/B4/B5~~ | ✅ **已通过（2026-10-22 晚）**：多轮往返无重复键、core ≥15 / format ≥15 断言齐备；B1/B2 验收解除，**解锁 B3**（B3 自身返修另计） |
| ~~B3 返修（16 fixture golden + 断言下限）~~ | ~~B4、B5~~ | ✅ **已通过（2026-10-22 晚）**：16 fixture 全部接入产品测试逐条断言 golden（含 08/14 linked 两路径、09/10/12/13 拒绝、11/15 约束）；binary 27→47、mapper 17→48；并以 option=ON 对真实语料 1113 单位独立复跑端到端闭环（region 未命中 0、8/8 提交重开一致、第二批能力从 unsupported 账本消失）。B3 验收通过，**B4/B5 不自动授权**，须逐个验收 |
| ~~V3 时基 / 帧率 / 舍入规则~~ | 动画与全部时间线；**以及 transform 的 mix / offset 语义解释**（D36） | ✅ **已定案 D39（2026-10-22）：默认 60 FPS；存储整数 tick/帧、舍入有理数/明确规则**。解除对动画时间线采样的时基阻塞；**D36「保留原值、不解释语义」的约束不变**，transform mix/offset 语义解释仍待真实样本 |
| ~~Q6-1 裁定~~ | Task 6 阶段 A | ✅ **已裁定 D40（2026-10-22）：复用同一份 Atlas/Texture 资产（指向同一文件），具体对象可选共用或独立化**；与 D25 不冲突 |
| **Runtime 存在**（Phase 3） | Phase 4 蒙皮 / IK / deform | 动画与求值落地 |
| **稳定顶点 ID / 索引契约** | deform 时间线与网格顶点编辑 | 单独定契约（Phase 4 前置） |
| **真实样本** | 多皮肤换装 / 皮肤混合（T-B4）、`BoundingBox`（0 例） | 语料或其他来源出现样本；不得用合成 fixture 宣称兼容（D13）。**注**：`BoundingBox` 已由 **D49** 裁定**纳入 Task 7**（非权重口径），但**真实样本缺口不因实现而消除** —— 实现后仍须写「已实现 + 待真实样本验证」 |
| **独立任务 + 迁移方案** | `SkeletonNode`（会改 v1 `SceneNode::type` 枚举） | 另立任务（D27） |

### 4.3 内部顺序（Task 5 阶段 B 内部，不可乱序）

`B1 Domain` → `B2 格式 v3` → `B3 Reader/Mapper` → `B4 插件` → `B5 语料闭环`；
B5 必须最后，且须从**干净构建目录**运行（B2 改变 `ProjectData` / `ProjectDocument` 布局 ⇒ 增量链接会 ODR 违例、堆损坏，必须全量重编）。

---

## 5. 验收锚点（阶段 B 完成后由本对话执行）

- 干净构建目录、`ctest` 全绿、**0 编译警告**
- 既有 **core 116 / format 70** 断言零回归；新增断言 ≥ 75（下限）
- **多轮往返无重复键**：`decode → encode → decode → encode → decode`（≥ 2 次保存）不得出现 `kJsonDuplicateKey`
  —— B1/B2 的 P0 正是漏了这条，v2 路径下第一次保存不重复、**第二次**才炸（`Agent_Task/Task5_B1B2_Review.md` §3）
- 16 个 fixture 的 golden 逐条匹配，**含 09 / 10 / 12 / 13 的拒绝行为**
- Full **1113** / Primary **963** 分报；≥ 3 个真实样本完成 源 → Domain → 保存 → 重开 → 逐项比对（记录浮点容差）
- smoke **退出码只有 0 可作验收证据**；共享图集 2 个单位单独列出，**不得**包装成通过
- `D37` 的拒绝路径（权重和 ≤ 0 / 非有限）在语料 **0 例** ⇒ 不得称"语料已验证"

---

## 6. 维护约定

本台账是**视图**，不是契约。任何类型进入 / 离开"未实现"集合时，先改对应 Task 与
`DataType_Report.md`，再同步本表；两者冲突时**以 Task 与决策台账为准**。
