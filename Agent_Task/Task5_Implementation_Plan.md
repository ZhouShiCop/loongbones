# Task 5 阶段 B：具体实现方案（提案）

- 状态：**已由负责人批准（2026-10-22），阶段 B 执行中**。实现由**实现会话**推进；本对话只做验收与计划
- 开工前置 R1 已完成：B6 遗留改动已建基线提交
- 上游：`Agent_Task/Task5.md`（规格）、`Agent_Task/Task5_A6_Acceptance.md`（**阶段 A 已验收通过**）
- **角色边界**：本文件只作计划。实现由**实现会话**执行；本对话在其完成后继续承担验收
- 日期：2026-10-22

---

## 1. 交付目标与边界

把阶段 A 获批的第二批契约落地为可运行代码：**Domain 类型 → 原生格式 v3 → Spine 3.2.01 Reader/Mapper → 插件诊断 → 全语料闭环**。

**交付边界（沿用 Task 4，不得突破）＝「结构可读」**：

- ✅ 可以说：第二批数据被**解析**、**建模**、**持久化**、**重开后逐项一致**
- ❌ 不得说：可渲染、可蒙皮、可播放动画、可求值 IK / transform

**目标规模（Full 口径，来自 A1 取证）**：mesh **2,979** / weightedMesh **17,271**（加权顶点 **312,083**）/
类型 4 linkedMesh **0** / 类型 5 weightedLinkedMesh **2** / IK **2,707** / transform **208** / boundingbox **0**。
Primary 口径另行实测回填，**两个分母不得混用**（D12）。

**明确不做**：动画时间线与求值（被 V3 时基阻塞，且属 Phase 3）；IK / 变形 / 蒙皮**求解**；渲染、Qt GUI、视口；
DragonBones；移植或翻译 Spine Runtime 源码；复制或提交语料资产；破坏 Task 1/2/3/4 的公共契约与已复核断言。

---

## 2. 契约基线（已冻结，实现时不得边写边改）

| 类别 | 依据 |
|---|---|
| 裁定 | `DECISIONS.md` **D29–D38**（v3 升版、权重原值+校验、hull/edges 不落、boundingbox 占位、共享图集另立 Task 6、负权重合法、IK `enabled`、transform 原值、权重和 > 0、fixture 对齐语料且解析器两条路径都支持） |
| 模型提案 | `docs/skeleton/mesh-and-constraint-rfc.md`（6 类类型、字段/单位/稳定键、9 个新诊断码） |
| 格式提案 | `docs/format/native-project-v3-mesh.md`（v3 schema、`migrateV2ToV3()`、读取顺序与错误码） |
| 测试样本 | `docs/skeleton/fixtures/` **16 个** + README 的 golden 表（06/07/08/14/16 合法路径，09/10/12/13 拒绝路径，11/15 约束） |
| 取证 | `docs/skeleton/mesh-constraint-evidence.md`（顶点空间、权重和分布、`hull`/`edges` 出现率、约束取值域） |

**9 个新诊断码（只追加，不改既有码语义）**：`MESH_TRIANGLE_INDEX_OUT_OF_RANGE` /
`MESH_VERTEX_GEOMETRY_INVALID` / `MESH_UV_OUT_OF_RANGE` / `MESH_LINKED_PARENT_UNRESOLVED` /
`MESH_WEIGHT_SUM_INVALID` / `MESH_WEIGHT_BONE_UNRESOLVED` / `MESH_BINDING_COUNT_INVALID` /
`CONSTRAINT_TARGET_UNRESOLVED` / `CONSTRAINT_CHAIN_EMPTY`；非有限浮点**复用** `SKELETON_FLOAT_NON_FINITE`。

**契约若在实现中发现问题 → 停下走 RFC 修订**，不得在代码里偷偷改语义（规范 §4.2、Task 4 §3 同款纪律）。

---

## 3. 开工前置（checklist）

| # | 前置 | 说明 |
|---|---|---|
| **R1** | **B6 遗留改动建立基线提交** | `spine32_import_plugin.cpp` / `test_spine32_plugin.cpp` 至今未提交（`git log` 最新仍是 2026-10-21 MVP initial commit）。不提交则阶段 B 新改动与 B6 返修混在同一工作区，无法二分定位 |
| **R2** | 负责人批准阶段 B | 阶段 A 验收通过 ≠ 授权实现；**一个工作包的批准不自动授权下一个** |
| **R3** | 工具链与构建目录 | `cmake`（MinGW Makefiles）+ `mingw32-make -j8`；**使用干净构建目录**（如 `D:\Works\Spine\.builds\build_bX`），不复用历史缓存；运行测试 exe 必须带 mingw64/bin 的 PATH |
| **R4** | 全量重编纪律 | `ProjectData` / `ProjectDocument` 布局变更后必须**全量重编**，增量链接会造成 **ODR 违例**（表现为堆损坏）—— Task 4 B3 已踩过 |

---

## 4. 工作包

### B1 — Domain 第二批类型与校验

- **改动**：`domain/include/loongbones/domain/skeleton.hpp`、`domain/src/*.cpp`（`validateSkeleton()` / `Project::adoptData()` 路径）。
- **新增类型**：`MeshAttachment`、`WeightedMesh`（`vertexBindings`）、`LinkedMesh`（`parentKey = (slotId, attachmentName)`，
  **不复制几何**）、`IkConstraint`（`boneIds` 有序链 / `targetBoneId` / `mix` / `enabled` / `bendPositive`）、
  `TransformConstraint`（4×mix + 6×offset **原值承载**）；`BoundingBoxAttachment` **只留类型占位**（D32）。
- **约束**：
  - 附件仍是 `SkinEntry` 内嵌值对象、**不单独分配 ID**；约束是骨架级对象、**有稳定 ID**（宿主随机 ULID，D25）。
  - 校验判据集中在既有路径，与将来的写 API 共用；Domain **不依赖** RapidJSON / Spine Reader / Qt / 文件系统。
  - 不得改变 `RegionAttachment` 的既有语义与字段（只增不改）。
  - 顶点空间按 **N2 维持**：非加权 = 槽骨骼局部、加权每条 = 对应骨骼局部（**不是**骨架空间）。
  - 权重**保留源原值**（D30）；导入期只校验权重和**有限且 > 0**（D37）；单条负权重合法（D34）。
  - `hull` / `edges` **不落**（D31）。
- **验收判据**：类型字段/golden 断言齐备；既有 **core 116 / format 70 断言零回归**；新建对象的闭合校验（引用悬空必须拒绝）。

### B2 — 原生格式 v3（三读一写）

- **改动**：`format/include/loongbones/format/codec.hpp`（`kFormatVersion` / `kReaderMaxVersion` → **3**，
  `kReaderMinVersion` 保持 **1**）、`format/src/decode.cpp`（新增 `migrateV2ToV3()`，紧跟现有 `migrateV1ToV2()`）、
  `format/src/encode.cpp`、`format/include/loongbones/format/project_document.hpp`。
- **约束**：
  - 迁移链 **v1 → v2 → v3**；`migrateV2ToV3()` 必须是**纯函数**（不改旧字段、不生成 ID、不碰磁盘、可重复执行）。
  - **写出统一为 v3**；v1 / v2 Reader 遇到 `formatVersion = 3` **固定返回 `FORMAT_VERSION_UNSUPPORTED`**，
    不降级打开、不静默丢字段。
  - `capabilities.required` 沿用 v2（atlas.v1 + skeleton.v1），**不新增**（D29）。
  - `Skeleton` 补 `ikConstraints: []` / `transformConstraints: []`（**键必须存在**，可为空数组）。
  - `OwnerKind` 新增 `kIkConstraint` / `kTransformConstraint`，沿用 D22「`(kind, 稳定 ID)`」位置无关锚点；
    解析到未知附件类型 / 未知约束种类时**显式拒绝 + 可定位诊断**。
  - B3 的 v2 fixture **保留为历史 fixture**，不得删除或改写。
- **验收判据**：v3 文档 encode→decode→encode 语义往返；未知字段 round-trip；锚点在重排后仍归属原对象；
  宿主删除对象后丢弃其锚点记录；v1/v2 fixture 仍可读；v1/v2 Reader 对 v3 的拒绝行为固定。

### B3 — Spine Reader / Mapper 扩展（**工作量最大**）

- **改动**：`importers/spine32/src/binary_reader.cpp`（现 899 行，当前只解析 region，mesh 族与约束**读完即弃**）、
  `source_model.hpp`（`UnsupportedCounts` 的 mesh / linkedMesh / weightedMesh / weightedLinkedMesh / boundingBox /
  ikConstraints / transformConstraints 从"只计数"改为真实 IR）、`mapper.cpp`（现 294 行）。
- **约束**：
  - **不得重造 B4 已验证的部分，不得放宽其 strict EOF / 边界 / 索引断言**。
  - 6 类附件全部落地（region / mesh / weightedMesh / 类型 4 / 类型 5 / boundingbox 占位）；
    **D38**：linked 的**两条路径都要支持** —— 显式 `parentSkin`（语料 0 例，fixture 08）与
    `parentSkin = null` 隐式同皮肤（语料 2 例，fixture 14）。
    ⇒ 需要**两阶段解析**（先收集全部 skin 与附件键，再解析父引用），单趟即时查找会重演旧 Reader 的失败。
  - `hull` / `edges` 按 D31 **不解析进 canonical**，遇到则计入 `report.unsupported` 账本，不建对象。
  - boundingbox 按 D32：遇到产出「不支持 + 待验证」诊断，**不建 Domain 对象、不留半成品**。
  - 加权顶点按 A1 证据：`boneCount` / `boneIndex` 是 **float 编码**（非官方 varint），已在 B4 证伪并修正，不得沿用官方写法假设。
  - region 查找键仍是 `path ?? name`（B5 规则）；未命中策略不变：`ATTACHMENT_REGION_DEGRADED`（warning）/
    `ATTACHMENT_REGION_UNRESOLVED`（error）。
  - **字节层坑（已踩过，勿复现）**：多字节数值是**大端**；`count()` 必须带 `kMaxArrayCount` / `kMaxStringBytes` /
    `kMaxBonesPerVertex` 护栏；region（类型 0）**没有前置 color**（`path + 7×f32 + color`），mesh 族才有 → 多读一个 i32 会全段错位。
- **验收判据**：16 个 fixture 的解析结果逐条匹配 README golden（含 09/10/12/13 的**拒绝**）；
  全语料扫描失败数不高于 B4 基线；`UnsupportedCounts` 中本批条目按预期下降。

### B4 — 插件诊断与报告

- **改动**：`importers/spine32/src/spine32_import_plugin.cpp`（诊断码映射与 `report.unsupported` 账本）。
- **约束**：`prepare()` / `commitImport()` 的**事务顺序、回滚范围、授权根校验不得改变**（B6 语义）；
  重复导入仍创建**新 ID 副本**（D25）；不提供插件版本迁移钩子（D28）。
- **验收判据**：第二批能力不再出现在"不支持"账本（boundingbox 除外，D32 仍记"待验证"）；
  B6 的 27 项宿主/插件断言**零回归**。

### B5 — 语料闭环与回归

- 从**干净构建目录**运行全部既有测试 + 新增测试；报告**精确命令、工具链、总数/通过/失败/跳过、退出码、编译警告**
  （规范 §4 第 6 条）。**不得只运行旧二进制**，不得复用历史缓存目录。
- Full **1113** / Primary **963** **双口径分别报告**，不得混用分母（D12）。
- 至少 **3 个特征不同**的真实样本完成 `Spine source → Domain → Native save → reopen → semantic comparison`；
  顶点 / UV / 三角形 / 权重 / 约束参数逐项比对，记录**浮点容差**与已知差异。
- 端到端 `prepare → commit → reopen` 后第二批对象计数与关键字段逐项一致。
- 失败原子性：注入解析/映射失败，验证目标工程前后快照一致、无半成品实体或临时资源。
- **退出码口径沿用 B6：只有 0 可作验收证据**；已知缺口（共享图集 2 个单位，已另立 Task 6）必须单独列出，
  **不得**包装成通过。
- 新增/改动的验证工具必须就近 README 并登记根 `STRUCTURE.md`（规范 §8.2）。

---

## 5. 测试矩阵（下限，非承诺）

| 目标 | 现状断言 | 本阶段新增（建议下限） | golden 来源 |
|---|---|---:|---|
| `loongbones_core_tests` | 116 | **≥ 15**（类型字段、校验拒绝、闭合） | RFC §4–§9 |
| `loongbones_format_tests` | 70 | **≥ 15**（v3 往返、迁移纯函数、v1/v2 拒绝 v3、锚点） | v3 提案 §3/§7 |
| `loongbones_binary_read_tests` | 27 | **≥ 20**（6 类附件 + 约束 + 拒绝路径） | fixtures 06–16 |
| `loongbones_spine32_mapper_tests` | 32 | **≥ 15**（源 IR → canonical） | fixtures + A1 证据 |
| `loongbones_skeleton_import_tests` | 27 | **≥ 10**（端到端、账本、拒绝） | fixtures、B6 基线 |
| `loongbones_atlas_import_tests` | 8 | 0（本批不触及） | — |

- 拒绝路径**必须**覆盖：三角形索引越界、`bones[]` 索引越界、权重和 = 0、权重和 < 0、权重个数超限、
  非有限浮点、IK 链为空 / 自环 / 目标越界、transform 骨骼越界、linked 父附件不存在。
- **BoundingBox 按 D32 不在功能测试范围**，只验证「遇到该类型产出不支持诊断、不建对象」。

---

## 6. 数字纪律与证据口径（阶段 B 全程适用）

1. **Full 1113 / Primary 963 双口径不得混用**，报告必须标明分母。
2. **smoke 退出码只有 0 可作验收证据**；非 0 必须说明是哪一个已知缺口。
3. **不得称「语料已验证」**：D37 的拒绝路径在真实语料里 **0 例**（`weight-sum-census.mjs`：1113/1113 单位、
   加权顶点 312,083，`sum ≤ 0` 与非有限均 0 例）⇒ 只有合成 fixture 覆盖（12 / 13）。
   同理 boundingbox（语料 0 例）、UV 越界（0 例）、类型 4 linked（0 例）都不得写成"已验证支持"（D13）。
4. 语料**只读**，不得写入或整理；不得提交任何游戏资产。
5. 「可导入 / 可解析」不得表述为「可渲染 / 可播放 / 可求值」。

---

## 7. 已知坑与风险（继承 Task 4 / 阶段 A，勿复现）

| 坑 | 表现 | 规避 |
|---|---|---|
| 布局变更后的增量链接 | 堆损坏（ODR 违例） | `ProjectData` / `ProjectDocument` 变更后**全量重编** |
| 二进制字节序 | 整段错位、绝大多数单位在文件末尾报 STR/COUNT | 多字节数值按**大端** |
| `count()` 无护栏 | 读到巨大长度后空转到超时 | 带 `kMaxArrayCount` / `kMaxStringBytes` / `kMaxBonesPerVertex` |
| region 附件布局 | 全语料错位 | region **无前置 color**；mesh 族才有 |
| linked 父引用 | 旧 Reader「找不到父」⇒ 继承字段全是假值 | 产品侧**两阶段解析**；linked 的继承字段不得作建模依据 |
| 显式 `null` 被 `??` 吞掉 | 「父在同皮肤」这条真实路径写不出来 | 判空用 `hasOwnProperty` / `std::optional`，不用 `??` 兜底 |
| 图集诊断码一律映射成解析错误 | 3333 条能力告警被计成解析错误 | 能力缺口 ≠ 解析失败（B6 已修，勿回退） |
| 编译耗时 | 全新全量约 **20 分钟** | 预留；增量 1–3 分钟/次 |

**规模估算（按代码量，非日历承诺）**：产品代码约 **1,400–2,000 行** + 测试约 **1,400–1,800 行**。

---

## 8. 允许与禁止修改范围

**允许**：阶段 A 文档（如需随实现修订，走 RFC 修订记录）、`domain/` 第二批类型、`importers/spine32/`、
`format/`（按 D29 的 v3 扩展）、合成 fixture 与其生成器、相关测试 / CMake / 工具 README / `STRUCTURE.md`。

**禁止**：动画时间线与求值；IK / 变形 / 蒙皮求解；渲染、Qt GUI、视口；DragonBones；复制或提交语料资产；
移植或翻译 Spine Runtime 源码；未经批准扩张原生格式；破坏 Task 1/2/3/4 的公共契约与已复核断言；
把「可导入」表述为「可渲染/可播放」；顺带改写无关代码或全仓格式化。

---

## 9. 授权与状态

| 包 | 状态 |
|---|---|
| 阶段 A（A1–A6） | **已验收通过**（`Agent_Task/Task5_A6_Acceptance.md`） |
| 开工前置 R1（B6 基线提交） | **已完成**（2026-10-22 基线提交） |
| 阶段 B 授权 | **已由负责人批准（2026-10-22）** |
| **B1 + B2** | **返修完成（2026-10-22），待复核**：P0 三处已知键补全 + 附件键表**按 type 分派** + 多轮往返无重复键回归断言；G1 core **+19** / format **+16**；G2 约束未知字段改为**可读可写**（嵌套子路径 decode/encode 逐字一致）。实测：干净构建零 error / 零 warning、ctest 7/7、core 135/135、format 86/86、binary 27/27；骨架导入 smoke **8/8 完整提交（含落盘 v3 → 重开）通过**，退出码 7 = 已知共享图集布局缺口（Task 6），非本次引入。原始复核结论：代码完成（干净构建零 error / 零 warning；ctest 7/7；core 116/116、format 70/70 零回归 —— 复核已复现），但实测发现 **P0：`decode.cpp` 的 `kSkeletonKeys` / `kAttachmentKeys` 未同步 v3 新键 ⇒ 保存产生重复键 ⇒ 无第二批数据的工程第二次保存后打不开、含第二批数据的第一次保存后即打不开**（`kJsonDuplicateKey`）。新增断言（core ≥15 / format ≥15）**未写**，正是它掩盖了该缺陷。详见 `Agent_Task/Task5_B1B2_Review.md` |
| **B3** | **返修完成（2026-10-22，第二轮）**：复核判不通过的两个 P0 已闭合 —— ① **16 个 fixture 全部接入产品测试**（binary_read 字节层 + mapper 的 IR→canonical→Domain 拒绝），逐条断言 `fixtures/README.md` golden；② 新增测试达下限：binary_read **27→47**、mapper **17→32**。P1（D38 两路径）经 fixture 08/14 双向验证闭合；P2（拒绝路径）断言到**具体诊断码**。返修过程查出**两个真实缺陷**：① `bendPositive` 字节语义 —— 语料用 **0xFF 表示 false**（新探针 `ik-bendpositive-probe.mjs` 直读字节：1113/1113、2707 条，0x01×1348 / 0xFF×1359），官方 `!= 0` 会把 1359 个 false 读成 true ⇒ 改有符号判定（**未改 fixture 字节**）；② linked 的 region 来自**父附件** ⇒ mapper 加同 slot 的「附件名→regionId」预扫（计划所说「两阶段」的实际用武之地）。实测：零 error / 零 warning、ctest 7/7、smoke 完整提交 8/8、第二批能力已从「不支持」账本消失。**B4 / B5 未开工**。详见 `Agent_Task/Task5_B3_Report.md` |
| **B4** | **返修完成（2026-10-22，第二轮）**：复核（`Task5_B4_Review.md`）判不通过的两个 P0 —— **P0-1 达成**：`skeleton_import_tests` 新增 **12 个**真实 fixture 三件套集成测试（端到端 7 + 账本 1 + 拒绝 4），测试数 17→29；**P0-2 部分达成**：正向端到端与账本（第二批 6 项均为 0）已达成，**4 个拒绝路径未闭合**（插件层 `adoptData` 放行了 09/10/12/13，而同一 fixture 在 mapper 层确实被拒并报出 `MESH_*` ⇒ 差异在插件路径的 region 查找 / candidate 组成，已排除 Reader、Domain 校验、adoptData 本身；测试**保留为红**作为证据）。**P1 闭合**：smoke 在 `build_b4` 重跑，退出码 7 = 已知共享图集缺口（不包装成通过）。**P2 闭合**：`mingw32-make` 退出码回到 **0**（根因＝首次配置未启用 smoke 目标 + 缓存不完整）。ctest **6/7**（红的正是那 4 项）。详见 `Agent_Task/Task5_B4_Report.md`。**二次复核仍不通过**（2026-10-23，本对话）：P0-1 ✅、P1 ✅、P2 ✅ 已闭合，但 **P0-2 的 4 个拒绝路径未闭合** ⇒ ctest 6/7 不得判通过、**不授权 B5**。复核**修正了定位方向**：mapper 层 `domainRejectCode()` 用**空** `NameRegions({})` + 仅骨架 candidate，与插件层（真图集 + 含 textures/atlas 的 candidate）**非同口径**，故报告所称「已排除 Reader / Domain 校验 / adoptData 本身」**未获证明**；推论是「第二批附件未真正进入 candidate skeleton」，应改查映射是否产出附件。实测测试数 **18 → 30**（报告写 17→29 属口径偏差）。**三次复核通过（2026-10-23）**：根因定位为 `Status::ok()` 是**静态工厂**（`diagnostics.hpp:129`）⇒ `!status.ok()` 恒为 false ⇒ 插件的 Domain 校验闸门是**死代码**、静默吞掉全部校验失败；改为 `status.isOk()` 后 4 个拒绝路径由红转绿（断言未削弱）。独立复跑：cmake/make exit 0 无 warning、ctest **7/7**、skeleton_import **40/40**、smoke 1111/1113 + 8/8 重开一致 + exit 7（已知缺口，未包装）。**新发现的同类缺陷已修（2026-10-23 复核）**：`atlas_import.cpp:318` 已改 `isOk()` + 警示注释；全仓扫描**无残留 `.ok()` 代码误用**（仅两处注释）；产物确认重编（obj 05:23 > 源码 05:22）、重编 exit 0 无 warning、ctest **7/7**、`atlas_import_tests` 仍绿 ⇒ 闸门由死变活未破坏既有图集导入。**残留已闭合（2026-10-23 晚，负责人指派本对话完成）**：`tests/importers/spine32/test_atlas_import.cpp` 新增 `RegionExceedingPageIsRejectedByDomain`（页 `32,32` + 区域 `xy:20,0 size:20,20` 越出页宽）⇒ 断言 `!valid` + `kDomainRejected` + 消息含「矩形越出页边界」+ 提交不落盘；atlas 测试 **8 → 9 全绿**。**反向探针已验**：临时把 `isOk()` 改回 `ok()` 后该测试**立即失败**（8/9、exit 1）⇒ 断言**可被证伪**、确为闸门回归护栏；随后还原 `isOk()` 并重编复跑通过（ctest 7/7、atlas 9/9）。详见 `Agent_Task/Task5_B4_Review.md` §10.8 |
| **B5** | **完成（2026-10-23）**：干净构建目录 `build_b5`（未复用缓存），cmake 0 / make 0、**error 0 / warning 0**、ctest **7/7 exit 0**；断言 core 135 / format 86 / binary 47 / mapper 48 / skeleton_import 40 / atlas 9，全通过。**双口径分别报告**：Full **1113 ok 1113 fail 0**、Primary **963 ok 963 fail 0**（`binary-read-smoke` 退出码 **0**，manifest 锁定样本逐条 OK）。**逐项语义比对**（B5 新增到 `skeleton-import-smoke`）：`[deep-compare] items=18067 maxResidual=0 mismatches=0` —— 覆盖 uvs/vertices/triangles/绑定(boneId,x,y,weight)/parentKey/IK(mix,enabled)/transform(4×mix+6×offset)，native 的 double 往返**精确**（残差 0）。失败原子性沿用既有 3 条断言（零回归）。退出码口径：`skeleton-import-smoke` 仍为 **7**（共享图集缺口，Task 6），**不作验收证据**。工具登记见该工具 README。详见 `Agent_Task/Task5_B5_Report.md`。~~WIP 期的 `std::fabs` 编译阻塞~~：已用不依赖 `<cmath>` 的绝对值写法解决（当前可编译，make exit 0） **B5 复核通过（2026-10-24，本对话独立复跑）**：`build_b5` 内 make exit **0** 无 error/warning、ctest **7/7 exit 0**；`binary-read-smoke` 复跑 ⇒ Full **1113/1113** / Primary **963/963**、manifest 锁定样本 16/16、退出码 **0**；`skeleton-import-smoke` 复跑 ⇒ `[deep-compare] items=18067 maxResidual=0 mismatches=0`、完整提交 8/8 重开一致（骨架 8/图集 8/纹理 8/区域 243/骨骼 331）、region 未命中 0、退出码 **7**（共享图集已知缺口，**不作验收证据**，与报告同口径）。deep-compare 经**代码级核对**确为真逐项比对（uvs/vertices 容差、triangles/boneId 精确、权重按 **D30 原值**、parentKey/inheritDeform、IK、transform），非仅比数量。判据 7：工具 README 已登记，**根 `STRUCTURE.md` §2.12 原缺失、由本对话补登记**。**遗留待裁定**：`sourceVersion` 标成 kV2 与落盘 v3 不自洽（`atlas_import.cpp:329` / `spine32_import_plugin.cpp:378`）会触碰既有断言 ⇒ 未擅自改；BoundingBox 语料 0 例（D32）；共享图集 2 个单位归 Task 6。详见 `Agent_Task/Task5_B5_Review.md` |

**停点**：每完成一个工作包即停止等待复核，不得自行启动下一个。全部完成后给出完成报告与下一阶段建议，
**不得自行宣称视觉/渲染效果通过**（规范 §7：视觉判断留给用户）。
