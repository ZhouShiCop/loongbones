# Task 5 阶段 B：B5 复核（语料闭环与回归）

- 上游：`Agent_Task/Task5_B5_Report.md`（实现报告）、`Agent_Task/Task5_Implementation_Plan.md` §4 B5
- 日期：2026-10-24
- 复核人：验收会话（检查 / 验收 / 计划）
- 复核结论：**通过**（判据 1–6 实测通过；判据 7 的 `STRUCTURE.md` 根登记原缺失，已由本对话补上后闭合）

---

## 1. 复核方法

1. **独立复跑**：在 `build_b5`（实现会话建的干净目录，`CMakeCache` 确认为 `g++.exe` + smoke ON）
   重跑 `mingw32-make` 与 `ctest`，确认产物与当前源码一致。
2. **独立跑两个 smoke**：报告里「双口径」一项**没有留下日志文件**，故本对话自行复跑
   `binary-read-smoke`（双口径）与 `skeleton-import-smoke`（端到端 + 逐项比对）。
3. **代码级核对**：`逐项比对` 是 B5 的核心新增能力，仅看输出数字不足以下结论 ⇒
   通读 `tools/skeleton-import-smoke/src/main.cpp` 的 `compareAttachmentDeep` / `compareSkeletonDeep`，
   确认它真的逐字段比对，而不是只比数量。
4. **文档登记核对**（规范 §8.2）：工具就近 README 与根 `STRUCTURE.md`。

---

## 2. 实测证据（本对话复跑，非转述）

| 项 | 命令 / 对象 | 结果 |
|---|---|---|
| `mingw32-make`（`build_b5`） | 重跑 | exit **0**，日志**无 error / warning** |
| `ctest`（`build_b5`） | `ctest --output-on-failure` | **7 / 7 通过，exit 0** |
| **双口径** | `binary-read-smoke <语料根>` | exit **0**；`Full 1113 \| ok 1113 \| fail 0`、`Primary 963 \| ok 963 \| fail 0`；manifest 锁定样本 **16/16** 一致 |
| **端到端 + 逐项比对** | `skeleton-import-smoke <语料根> --limit 8` | `[deep-compare] items=18067 maxResidual=0 mismatches=0`；完整提交 **8/8**，重开后 骨架 8 / 图集 8 / 纹理 8 / 区域 243 / 骨骼 331；region 未命中 **0**；prepare 1111 / 失败 2（共享图集）；exit **7** |

---

## 3. 判据逐项核对

| # | B5 判据 | 结论 | 依据 |
|---|---|---|---|
| 1 | 干净构建目录 + 全量回归 + 报告命令/工具链/计数/退出码/警告 | ✅ | `build_b5` 为新建目录（未复用缓存）；报告给出完整 cmake/make/ctest 命令与工具链（g++ 16.2.0 / CMake 4.4.4）；复跑 make 0、ctest 7/7 exit 0、零 warning |
| 2 | Full 1113 / Primary 963 **双口径分别报告** | ✅ | 本对话复跑：`Full 1113 ok 1113`、`Primary 963 ok 963`，两行**分别列出未合并**（D12） |
| 3 | ≥3 个特征不同的真实样本逐项语义比对 | ✅ | 实际 **8 个**样本（≥3，骨 1…105、mesh 0…28，含加权与约束单位）；`items=18067 mismatches=0 maxResidual=0` |
| 4 | 端到端计数与关键字段一致 | ✅ | 8/8 重开后各计数逐项相同 + deep-compare 一致 |
| 5 | 失败原子性（注入失败 ⇒ 前后快照一致、无半成品） | ✅ 沿用既有断言 | `InvalidDraftCommitsNothing`（断言 `!committed` + `stagedCount==0` + 文件不存在）+ `StagingFailureRollsBackAndLeavesNoFile` / `PublishFailureRollsBackAndLeavesNoFile` / `SaveFailureRollsBackResources`，本轮零回归。**未新增**注入式测试（见 §4.2） |
| 6 | 退出码口径（只有 0 可作验收证据） | ✅ 处理得当 | `binary-read-smoke` **0** 与 `ctest` **0** 用作证据；`skeleton-import-smoke` **7** 明确标注为共享图集已知缺口、**不作验收证据**，且报告说明「退出码 7 的路径先于结论返回，不据以宣称闭环通过」 |
| 7 | 工具登记（就近 README + 根 `STRUCTURE.md`） | ⚠️ → ✅ | README 已加「逐项语义比对（Task 5 B5）」小节；**根 `STRUCTURE.md` 原未登记**（见 §4.1），已由本对话补入 §2.12 |

---

## 4. 发现与说明

### 4.1 判据 7：`STRUCTURE.md` 根登记缺失（已由本对话补齐）

规范 §8.2 要求改动过的验证工具**同时**登记就近 README **与**根 `STRUCTURE.md`。
实现会话只做了 README；`STRUCTURE.md` §2.12 的「输出 / 当前基线」仍是 B6 时期的描述，
**没有** deep-compare 能力、也没有 B5 的实测数字。

⇒ 本对话已补：`STRUCTURE.md` §2.12 的「输出」增列逐项比对覆盖字段与 `items/maxResidual/mismatches`
输出格式；「当前基线」增列 `items=18067 maxResidual=0 mismatches=0` 及「退出码 7 不得据以宣称闭环通过」的提醒。

### 4.2 判据 5：失败原子性沿用既有断言，未新增注入式测试

B5 原文要求「注入**解析 / 映射**失败，验证目标工程前后快照一致」。实现会话的做法是沿用
B6 的 3 条宿主回滚断言 + `InvalidDraftCommitsNothing`，并明示为本轮零回归。

复核评估：**可接受** —— 插件侧已有大量解析/映射失败路径（含 B4 新增的 09/10/12/13 拒绝）
⇒ 失败即 `draft.valid=false` ⇒ `commitImport` 不提交 ⇒ 与 `InvalidDraftCommitsNothing` 的语义正好闭合。
但严格说，缺少一条「**快照前后逐项相同**」的显式断言（现有断言验的是「没写出文件 / 暂存为 0」，
不是「工程内容与失败前逐字节相同」）。记为**可选增强**，不阻断验收。

### 4.3 deep-compare 确为真逐项（代码级核对）

通读 `main.cpp:171-270`：

- mesh：`uvs` 长度 + **逐元素残差**、`vertices` 逐元素残差、`triangles` **精确**相等；
- 加权 mesh：`vertexBindings` 长度 + 每顶点绑定数 + `boneId` **精确** + `x`/`y`/`weight` 残差
  （权重按 **D30 原值**比对，不归一化）；`triangles` 精确；
- linked：`parentSlotId` / `parentAttachmentName` / `inheritDeform`；
- 约束：IK 的 `boneIds` / `targetBoneId` / `enabled`（D35）/ `bendPositive` / `mix`；transform 的 4×mix + 6×offset（D36 原值）。

⇒ 不是「只比对象数」，判据 3 的「逐项」名副其实。`maxResidual=0` 也合理：native 用 double 文本往返是**精确**的。

---

## 5. 遗留项（不阻断 B5，但须单列）

| 项 | 说明 |
|---|---|
| **`sourceVersion` 的 kV2 遗留** | `atlas_import.cpp:329`、`spine32_import_plugin.cpp:378` 仍把内存文档标成 kV2，而落盘已是 v3 ⇒ 语义不自洽。改动会触碰既有断言（`test_spine32_plugin.cpp:350-351`）⇒ **等负责人裁定**，实现会话未擅自改，处理正确 |
| **BoundingBox** | 语料 **0 例**（D32）⇒ B5 无真实样本可跑，仅类型占位与诊断，无功能验证。不得写成「已验证支持」（D13） |
| **共享图集缺口** | 2 个单位（`h0164s5/watch/`），归 **Task 6**，不在 B5 范围；`skeleton-import-smoke` 退出码 7 即此 |
| **工作区未提交** | B1/B2 返修 + B3 + B4 + B5 全部未提交（停在 `049b1d3`），由负责人手动提交 |

---

## 6. 结论与停点

**B5 复核通过。** 判据 1–6 全部实测通过（含本对话独立复跑的双口径与逐项比对），
判据 7 的 `STRUCTURE.md` 根登记已由本对话补齐。

- **阶段 B（B1 → B2 → B3 → B4 → B5）至此全部完成并通过复核。**
- 下一阶段（Task 6 共享图集 / Phase 3 动画 / Qt GUI 等）**需负责人另行批准**，不自动开工。
- §5 的 `sourceVersion` kV2 语义问题**待裁定**，是本轮唯一需要负责人拍板的技术项。
- 数字纪律延续：Full 1113 / Primary 963 双口径不得混用；smoke 退出码只有 0 可作验收证据，
  退出码 7 属已知缺口、不得包装成通过。

---

## 7. 复核后补充：`sourceVersion` 统一为 kV3（负责人 2026-10-24 裁定「统一为 V3」）

§5 遗留的「`sourceVersion` 标 kV2 而落盘已是 v3」已按裁定修复。该字段语义是
「**磁盘上读入时的版本**」的内存标记（`decode.cpp:228` 按磁盘 `formatVersion` 置位），
`encode` 不写它；写出统一为 v3 后，新建 / 导入产出的文档再标 kV2 即语义不自洽。

### 7.1 改动清单（含既有断言改动，如实披露）

| 文件 | 改动 | 性质 |
|---|---|---|
| `format/include/loongbones/format/project_document.hpp:98-101` | 默认值 `kV2` → `kV3`，注释同步 | 产品（新建文档不再谎报 v2 来源） |
| `importers/spine32/src/atlas_import.cpp:332` | `kV2` → `kV3` | 产品 |
| `importers/spine32/src/spine32_import_plugin.cpp:380` | `kV2` → `kV3` | 产品 |
| `tests/importers/test_spine32_plugin.cpp:350-352` | 断言 `kV2` → **`kV3`** | **既有断言改动（1 处）** —— 该断言正是钉住插件产出文档的版本标记，随产品改动同步，非放宽 |
| `tests/importers/test_import_host.cpp:257` | 测试输入 `kV2` → `kV3` | 测试**输入**（非断言）：模拟插件产出的 draft，与产品对齐 |
| `format/include/loongbones/format/codec.hpp:73` | 修正过时注释 | 文档：原文写「标记 `sourceVersion = kV1`」，实际 `migrateV1ToV2` **不改**该字段（见 `decode.cpp:1610` 注释） |

**安全性核对（改前已确认无副作用）**：
- `encode.cpp` 完全不读 `sourceVersion` ⇒ 落盘内容不变；
- `decodeDocument` 在迁移**前**显式按磁盘版本置位 ⇒ 不受默认值影响；
- 全仓只有 2 处产品赋值 + 1 处默认 + 2 处测试引用；无测试断言「新建文档默认为 kV2」
  （`test_v2_skeleton.cpp` 的 kV1 / kV3 断言全部来自 decode 结果）。

### 7.2 改后复跑（全部与验收时一致，零回归）

| 项 | 结果 |
|---|---|
| `mingw32-make`（`build_b5`） | exit **0**，无 error / warning |
| `ctest` | **7 / 7，exit 0** |
| `skeleton-import-smoke` | exit **7**（已知缺口）；`[deep-compare] items=18067 maxResidual=0 mismatches=0`；完整提交 8/8；重开后 骨架8/图集8/纹理8/区域243/骨骼331；region 未命中 0 |
| `binary-read-smoke` | exit **0**；Full **1113/1113**、Primary **963/963**、锁定样本 16/16 |

⇒ 统一为 kV3 未改变任何落盘内容与语料行为。

### 7.3 其余「可自行决定」项的处置（本对话决定）

| 项 | 决定 | 理由 |
|---|---|---|
| 失败原子性补「快照前后逐项相同」的显式断言 | **暂不做** | B5 判据 5 已由既有断言闭合且通过；属可选增强，不是缺口。留待下一工作包按需补，避免验收后无边界扩写 |
| BoundingBox | **维持延后**（D32） | 语料 0 例，无真实样本可验；不得用合成样本宣称支持（D13） |
| 共享图集 2 个单位 | **维持归 Task 6** | 不在阶段 B 范围 |
| 工作区提交 | **仍由负责人手动提交** | 沿用既定分工 |
