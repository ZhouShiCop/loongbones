# Loongbones 项目结构

本文件是 `loongbones/` 的**目录职责索引**与**开发验证工具入口登记簿**
（规范 `项目规范.md` §8.2）。它记录稳定的目录职责和工具怎么跑，
**不维护易过期的全量文件清单**；工具细节与用法留在各工具目录同级的 `README.md`，本文件只引用。

> 命名待办：规范正文写 `loongbones/`，实际目录为 `Loongbones/`（Windows 大小写不敏感，
> 但跨平台会分叉）。**大小写需负责人定夺**，定稿前本文件沿用实际目录名 `Loongbones/`。

---

## 1. 当前状态

**Phase 0 已完成**（数据盘点、许可、决策台账）。Task 2 已完成：Scene/root 单场景契约、Atlas 全量校验（含旋转 trim 方向）、Sprite→Region 公开 API 均闭合；67 项测试从干净目录 `build_t5` 重建通过（`ctest` 1/1，0 警告），失败导入路径对已持有有效数据的接收对象做完整前后快照断言；见 `Agent_Task/Task2_Back_task.md` 与 `Agent_Task/Task2_Report.md`。

顶层：

| 路径 | 职责 | 状态 |
|---|---|---|
| `项目规范.md` | 产品与工程规格（负责人撰写，唯一权威） | 已确立 v0.1 |
| `docs/` | 设计文档、ADR/RFC、报告、许可清单 | Phase 0 产出中 |
| `docs/format/` | 原生格式 Schema v1 RFC、机器可读 schema 与合成 fixture | Task 3 阶段 A **已审阅**；RFC v1.2 复审通过，阶段 B 已获准启动 |
| `format/` | 原生项目格式 v1 codec：严格解析（RapidJSON SAX）、解码、编码、目录工程原子保存 | Task 3 阶段 B 已落地。依赖 `domain`；RapidJSON 仅出现在 `.cpp`，不进公开头文件 |
| `importers/api/` | Task 4 B6 导入插件契约：`IImportPlugin`、`ImportRequest` / `ImportDraft` / `ImportReport`、诊断码、宿主服务接口（`IResourceResolver` / `IResourceStaging` / `IIdFactory` / `IProgressSink` / `ICancelToken`） | **实现与验证完成，待负责人复核**；只依赖 `domain` 与 `ProjectDocument`，无 GUI / 无 ABI |
| `importers/host/` | Task 4 B6 静态插件宿主：`ImportRegistry` + `commitImport()` 事务提交 | **实现与验证完成，待负责人复核**；只依赖 `api/`，不依赖任何具体插件 |
| `importers/spine32/` | Task 4 B1/B5/B6 Spine 3.2.01：Atlas 文本 Reader、Source→canonical Mapper、PNG 头检查、插件 `prepare()` 与文件系统服务实现 | **实现与验证完成，待负责人复核**；不含 Runtime 依赖 |
| `vendor/rapidjson/` | RapidJSON v1.1.0（MIT）头文件 + 许可证全文 | 固定 commit `f54b0e47a08782a6131cc3d60f94d038fa6e0a51`；来源自证见 `VENDOR.md`，已登记 `docs/licenses-and-risks.md` |
| `tools/format-tests/` | codec 自动化测试（47 项，`ctest -R format_tests`） | Task 3 阶段 B；含 Back Task 的 §2.1 原始数值 token 长度上限、§2.2 越界数值个数无内部常量上限两项回归；与只读探针 `format-probe` 分工互补，见其 README |
| `tools/atlas-import-tests/` | Task 4 B1 Atlas + PNG 导入自动化测试 | `cmake --build build_b1 --target loongbones_atlas_import_tests`；`ctest --test-dir build_b1 -R atlas_import_tests --output-on-failure`；用法与覆盖见同目录 README |
| `tools/atlas-import-smoke/` | Task 4 B1 无 GUI Atlas + PNG 导入入口 | `cmake --build build_b1 --target loongbones_atlas_import_smoke`；参数、安全边界和 warning 确认见同目录 README |
| `docs/skeleton/B1_REPORT.md` | Task 4 B1 Atlas + PNG 首片交付、行为边界与验证证据 | **实现与验证完成，待负责人复核** |
| `docs/skeleton/` | 骨架模块设计（Task 4）：Skeleton Domain 与原生格式兼容提案、插件宿主契约、字段映射表、样本 manifest、合成 fixture、验收报告 |
| `Agent_Task/DataType_Report.md` | **Loongbones 数据类型报告**：已实现 canonical 类型总表、被误删重建文件的符号/字段核对结果、后续待实现类型（含语料实测规模）与残余风险 | **阶段 A 与 B0 复核通过；B1 实现与验证完成、待复核；B2/B3 实现完成；B4 Reader 全语料 1113/1113 精确到达 EOF、27 项测试通过（交付边界＝结构可读）；B5 Atlas Reader + Mapper 完成、32 项测试、全语料 region 未命中 0；**B6 已于 2026-10-22 复核通过**（阶段 B 收尾）**。报告见 `docs/skeleton/B1_REPORT.md`、`B3_REPORT.md`、`task4_B4.md`、`B5_REPORT.md`、`B6_REPORT.md` |
| `docs/skeleton/canonical-visual-data-types.md` | Canonical 视觉类型与功能导向建模：TextureAsset、AtlasPage、AtlasRegion、Attachment、Mesh/UV、绑定/蒙皮分层候选 | **B0 类型边界与首批功能对照表复核通过（§6 / §7.0）**；首片仅 Atlas + PNG，不读骨架；绑定等后续能力需先完成跨格式证据分析 |
| `docs/format/native-project-v2-skeleton.md` | 原生格式 v2（Skeleton 增量）Schema 冻结稿：完整 JSON 示例、字段必需性/默认值、枚举字符串、ID 规则、未知字段锚点、required capability、迁移与错误码、资源落地策略、诊断等级 | Task 4 阶段 B **B0 复核通过**；不改写已审阅的 v1 RFC |
| `docs/skeleton/mesh-and-constraint-rfc.md` | **Task 5 A2**：第二批附件与约束 canonical 模型提案（mesh / weightedMesh / linkedMesh / boundingbox 占位 / IK / transform；含顶点空间证据与 golden 计划、TBD-A…TBD-F） | **阶段 A 提案，待负责人审阅**；实现归阶段 B |
| `docs/format/native-project-v3-mesh.md` | **Task 5 A3**：原生格式 v3 Schema 提案（v2 + 第二批骨架类型增量；`formatVersion = 3`、迁移链 v1→v2→v3、不引入新 capability，D29） | **阶段 A 提案，待负责人审阅**；不改写 v2 冻结稿 |
| `Agent_Task/Task5.md` | Task 5 第二批静态数据建模与导入规格（含 A1 取证报告、复核记录、A2/A3 提案指针） | **阶段 A 六项全部完成并验收通过（2026-10-22，见 `Agent_Task/Task5_A6_Acceptance.md`）**；**阶段 B 已于 2026-10-22 批准并开工（实现会话执行中）** |
| `Agent_Task/Task5_Implementation_Plan.md` | **Task 5 阶段 B 方案**：B1 Domain 第二批类型 → B2 原生格式 v3（三读一写）→ B3 Reader/Mapper → B4 插件诊断 → B5 语料闭环 | **已批准（2026-10-22），阶段 B 执行中**；R1 已满足（B6 基线已提交） |
| `Agent_Task/Task5_A6_Acceptance.md` | Task 5 阶段 A 验收记录（交付物、停点纪律、一致性 5 项、未决项、阶段 B 开工前置） | **阶段 A 验收通过** |
| `Agent_Task/Task6.md` | Task 6 共享图集布局（B6 已知缺口，2 个单位）独立规格 | 待负责人裁定 Q6-1…Q6-3 |
| `tools/skeleton-probe/` | Spine 3.2.01 骨架契约只读探针 + 合成 fixture 生成器（登记见 §2.4） | Task 4 阶段 A 已交付 |
| `docs/scene-system/` | 独立场景系统基础模型设计及后续实现规格 | 基础设计已记录；实现待排期 |
| `Agent_Task/Task1.md` | Task 1 核心数据模型任务与现行场景层级契约 | 原任务完成；其中父级限制由新契约取代，Task 2 更新实现 |
| `Agent_Task/Task2.md` | 场景领域模型与通用节点层级执行规格 | 已完成 |
| `Agent_Task/Task2_Back_task.md` | Task 2 Scene/root、Atlas 校验与 Sprite API 返修规格 | **已返修并验收** |
| `Agent_Task/Task3.md` | 原生项目格式 Schema v1 与 JSON Codec 执行规格 | **阶段 A 已审阅；RFC v1.2 复审通过，阶段 B 已获准启动** |
| `Agent_Task/Task3_Back_task.md` | Task 3 超范围数值长度上限与固定重试次数边界返修规格 | **已返修并复核通过** |
| `Agent_Task/Task4.md` | Spine 3.2.0.1 导入插件、宿主 API 与骨架数据契约 | **阶段 A 与 B0 复核通过；B1–B5 已核验通过；B4 Reader 全语料 1113/1113 通过（结构可读）；**B6 已于 2026-10-22 复核通过**（阶段 B 收尾）** |
| `Agent_Task/Task4_Implementation_Plan.md` | Task 4 阶段 B 方案（B0 数据类型/Schema/资源策略冻结；后续 Atlas+PNG、Skeleton/Codec/Reader/Mapper/宿主事务） | **B0 复核通过；B1/B2/B3 已实现；B4 Reader 全语料 1113/1113 通过（结构可读）；B5 Atlas Reader + Mapper 已实现；**B6 已于 2026-10-22 复核通过**（阶段 B 收尾）** |
| `Agent_Task/Task4_Back_task.md` | Task 4 阶段 A 文档契约复核返修规格 | **复核通过，已关闭** |
| `Agent_Task/Back_task.md` | Task 1 父级类型约束历史退回记录 | **已被现行可挂载规则取代** |
| `tools/` | 开发验证工具（核心测试、语料扫描器、格式探针等） | Phase 0/Phase 1 工具 |
| `CMakeLists.txt` | 顶层构建（C++17 + CMake） | 已建 |
| `domain/` | Qt 无关的领域模型；Task 1 已建，Task 2 扩展 Scene/Node/图集模型，Task 4 B2 新增骨架模型 | **已建；Task 2 已完成；B2 骨架层已建（`skeleton.hpp` + `skeleton.cpp` 校验），B0 契约已落地** |
| `tests/` | 自动化单元测试源码（按被测模块分目录；构建入口在 `tools/*-tests/`） | **已建**：`tests/domain/`（core 116）、`tests/format/`（23，与 `tools/format-tests/src/main.cpp` 的 47 项合并为 `format_tests` 的 70 项）、`tests/importers/spine32/`（atlas-import 8 + binary-read 27 + atlas 文本 Reader 16 + Mapper 16）、`tests/importers/`（宿主事务 10 + 插件集成 15）。**全绿**。注意：atlas-import 与 binary-read 两组文件在 2026-10-21 的目录整理中被误删后已**重写**（无 git 可还原），文件头有重写说明 |
| `app/` `runtime/` `render/` `ui/` | 其余源码分区（规范 §8.2） | **未创建**，按阶段推进（`format/` 与 `importers/` 已创建，见上） |
| `DECISIONS.md` | 决策台账：已定案 / 待验证 / 待确认 | Phase 0 产出 |

源码分区按规范 §8.2 的建议命名，但**是否采用、以及语言/构建系统，属 Phase 1 ADR-0001 待决项**；
在负责人签定前不预先创建空目录、不假定技术栈（规范 §8.1）。

---

## 2. 开发验证工具登记簿

每个工具都必须在此登记；新增/移动/改名/弃用/删除工具时，同一改动更新本文件与该工具的使用说明。

登记共 13 项（§2.1–§2.13），按编号顺序：`corpus-scan`、`core-tests`、`format-probe`、
`skeleton-probe`、`atlas-import-tests`、`atlas-import-smoke`、`binary-read-tests`、
`binary-read-smoke`、`spine32-mapper-tests`、`spine32-mapper-smoke`、
`skeleton-import-tests`、`skeleton-import-smoke`、`format-tests`。

### 2.1 `tools/corpus-scan/` — Sdorica Spine 3.2.01 语料盘点

| | |
|---|---|
| 相对路径 | `tools/corpus-scan/` |
| 用途 | Phase 0 只读清单：SHA-256、大小、版本、骨骼/插槽/动画/IK/mesh/加权统计；963 vs 1113 口径分析；首批样本选择 |
| 适用场景 | 建立回归分母、解释语料口径、挑选验收样本 |
| 使用说明 | [`tools/corpus-scan/README.md`](tools/corpus-scan/README.md) |
| 运行入口 | 见下方命令 |
| 输入 | 语料根目录（**只读**），由 `$LOONGBONES_CORPUS_ROOT` 或首参数提供 |
| 输出 | `tools/corpus-scan/out/`（生成目录，可删可重建） |
| 依赖 | Node.js；旧项目 Reader `converter/src/read/binary32.ts`、`converter/src/model.ts`（**分析工具借用，非产品代码**） |
| 可安全重跑 | 是（输出确定：按路径排序 + 纯聚合） |
| 耗时 | 全量约 1–2 分钟 |

**入口一：全量扫描**

```bash
LOONGBONES_CORPUS_ROOT="<语料根>" node tools/corpus-scan/scan.mjs
# 产出 out/corpus-manifest.json、out/corpus-manifest.md、out/corpus-failures.json
```

**入口二：口径分析（963 vs 1113）**

```bash
node tools/corpus-scan/analyze.mjs    # 产出 corpus-reconciliation.md
```

**入口三：首批样本选择**

```bash
node tools/corpus-scan/select-samples.mjs    # 产出 sample-selection.md、out/samples.json
```

**注意**：三个入口都必须在**旧项目根**（`D:/Works/Spine`）下运行，脚本用 `../../..`
定位旧项目根来 import Reader。语料是《万象物语》专有资产，**只读**，
不得复制进本仓库或构建产物；manifest 内只有相对路径 + hash + 统计量，不含资产本体。

---

### 2.2 `tools/core-tests/` — 核心模型单元测试说明

| | |
|---|---|
| 相对路径 | `tools/core-tests/` |
| 用途 | 说明如何构建与运行单元测试（116 用例）及覆盖范围 |
| 使用说明 | [`tools/core-tests/README.md`](tools/core-tests/README.md) |
| 运行入口 | `cmake -S Loongbones -B Loongbones/build -G \"MinGW Makefiles\"` 后 `cmake --build Loongbones/build`，再跑 `Loongbones/build/tests/loongbones_core_tests.exe` |
| 输入 | 无（纯内存构造的工程数据） |
| 输出 | 终端打印逐用例 PASS/FAIL 与汇总；退出码 0 为全通过 |
| 依赖 | CMake + MinGW-w64（见 `.devtools/README.md`）；**零第三方测试库** |
| 可安全重跑 | 是（ID 随机，但断言只校验合法性与唯一性） |

### 2.3 `tools/format-probe/` — 原生格式 Schema v1 阶段 A 探针

| | |
|---|---|
| 相对路径 | `tools/format-probe/` |
| 用途 | 只读验证 `docs/format/native-project-v1-rfc.md` 的严格 JSON、字段/版本/能力与领域不变量判据；不属于正式 codec |
| 使用说明 | [`tools/format-probe/README.md`](tools/format-probe/README.md) |
| 输入 | `docs/format/fixtures/` 合成合法/非法 JSON fixture，或显式指定的 JSON 文档 |
| 输出 | 逐文件诊断（code、JSON Pointer/偏移、说明）；`--selftest` 退出码 0 表示全部 fixture 与预期一致 |
| 依赖 | C++17、CMake；Qt-free、零第三方库 |
| 可安全重跑 | 是；只读输入，不修改文件 |

### 2.4 `tools/skeleton-probe/` — Spine 3.2.01 骨架契约只读探针（Task 4 阶段 A）

| | |
|---|---|
| 相对路径 | `tools/skeleton-probe/` |
| 用途 | 为骨架 Domain/插件宿主 RFC 提供**可复现的数值证据**：版本/三件套、对象计数、引用闭合、名称唯一性、附件 `path` 语义、图集形态（rotate/trim/pma/format）、附件→region 查找率、图集页坐标空间像素判据；**Task 5 A1 第二批静态数据取证**（加权顶点骨骼数/权重和/索引越界、顶点空间判据、`hull`/`edges` 出现率、boundingbox 0 例、LinkedMesh 逐条、IK / transform 取值域）；并生成**合成 fixture**（不含游戏资产） |
| 适用场景 | Task 4 阶段 A 的证据产出；阶段 B 用其中的合成 fixture 做导入插件回归 |
| 使用说明 | [`tools/skeleton-probe/README.md`](tools/skeleton-probe/README.md) |
| 运行入口 | `node tools/skeleton-probe/probe.mjs <语料根> -o tools/skeleton-probe/out`；`node tools/skeleton-probe/png-page-probe.mjs <语料根> --limit 24`；`node tools/skeleton-probe/make-fixture.mjs --verify`；`node tools/skeleton-probe/emit-samples.mjs`（样本 manifest 数据表）；`node tools/skeleton-probe/vert-probe.mjs <相对路径> <起始偏移> <顶点数> <目标结束偏移>`（在给定偏移上试各种顶点区布局，看哪种正好落在目标结束偏移）；`node tools/skeleton-probe/mesh-constraint-probe.mjs <语料根> -o tools/skeleton-probe/out`（Task 5 A1 第二批数据取证，Full/Primary 双口径）；`node tools/skeleton-probe/tc1-space-control.mjs <语料根>`（Task 5 A1 顶点空间判据的**对照实验**，用已知答案样本自检判据）；`node tools/skeleton-probe/type-byte-census.mjs <语料根> -o tools/skeleton-probe/out`（Task 5 A1 **TBD-F**：附件类型字节直接普查，不复用旧 Reader，按 `binary_reader.cpp` 已验证布局独立走字节）；`node tools/skeleton-probe/weight-sum-census.mjs <语料根>`（Task 5 A5 **D37**：加权顶点权重和分布普查 —— `sum=0` / `sum<0` / 非有限 / 离差计数与 min/max） |
| `vert-probe.mjs` | 走查专用：语料根**不写死在源码里**，由 `$LOONGBONES_CORPUS_ROOT` 提供（规范 §8.2）；缺参即退出码 2 |
| 输入 | 语料根目录（**只读**），由 `$LOONGBONES_CORPUS_ROOT` 或首参数提供；fixture 模式输入 `docs/skeleton/fixtures/` |
| 输出 | `tools/skeleton-probe/out/`（生成目录，可删可重建；含 `mesh-constraint-probe.json/.md`、`type-byte-census.md`；权重和普查只打印到 stdout）；`docs/skeleton/fixtures/`（**入库**的合成 fixture，**16 个**：5 个首批 Task 4 + 11 个第二批 Task 5 A5 —— 最小 mesh(nonessential=1) / 最小 mesh(nonessential=0，语料主流) / 加权含负权重 / linked 类型4 / linked 类型5（语料同构）/ 三角索引越界 / 骨骼下标越界 / 权重和=0 / 权重和<0 / 约束 mix=1 / 约束 mix=0） |
| 依赖 | Node.js v22+；旧项目 Reader `converter/src/read/binary32.ts`、`converter/src/model.ts`、`converter/src/atlas.ts`（**分析工具借用，非产品代码**）；零第三方库 |
| 可安全重跑 | 是（按路径排序 + 纯聚合，输出确定） |
| 纪律 | 只读语料；输出只含相对路径 + hash + 计数，**不写绝对路径、不复制/落地任何游戏资产** |

### 2.5 `tools/atlas-import-tests/` — Spine Atlas + PNG 导入专项测试（Task 4 B1）

| | |
|---|---|
| 相对路径 | `tools/atlas-import-tests/` |
| 用途 | 验证 Atlas/PNG 映射、路径授权、警告门槛、资源提交回滚及原生工程重开 |
| 使用说明 | [`tools/atlas-import-tests/README.md`](tools/atlas-import-tests/README.md) |
| 运行入口 | `cmake --build build_b1 --target loongbones_atlas_import_tests`；`ctest --test-dir build_b1 -R atlas_import_tests --output-on-failure` |
| 输入 | 内存合成 Atlas/PNG fixture；无游戏资产依赖 |
| 输出 | 8 项用例 PASS/FAIL；全通过时退出码 0 |
| 依赖 | C++17、CMake、MinGW-w64；复用项目内测试框架 |
| 可安全重跑 | 是；临时工程在系统临时目录创建并由测试清理 |

### 2.6 `tools/atlas-import-smoke/` — 无 GUI Atlas + PNG 导入演练入口（Task 4 B1）

| | |
|---|---|
| 相对路径 | `tools/atlas-import-smoke/` |
| 用途 | 对用户显式授权的源目录执行 Atlas/PNG 导入，展示 warning 确认门槛并保存原生工程 |
| 使用说明 | [`tools/atlas-import-smoke/README.md`](tools/atlas-import-smoke/README.md) |
| 输入 | 授权源根目录、相对 Atlas 路径、目标工程目录；源目录只读 |
| 输出 | 项目 `project.json` 与 `assets/textures/<TextureAssetId>.png`；未确认 warning 时不写项目文件 |
| 依赖 | C++17、CMake、MinGW-w64；现有 Domain/Format 模块 |
| 可安全重跑 | 对新目标目录可重跑；重复导入会创建新 ID 的副本 |

### 2.7 `tools/binary-read-tests/` — Spine 3.2.01 二进制 Reader 测试（Task 4 B4）

| | |
|---|---|
| 相对路径 | `tools/binary-read-tests/` |
| 用途 | 27 项自动化测试：合成 fixture 与畸形输入，逐条锁住 Reader 的规则 |
| 使用说明 | [`tools/binary-read-tests/README.md`](tools/binary-read-tests/README.md) |
| 运行入口 | `cmake --build <build> --target loongbones_binary_read_tests`；`ctest --test-dir <build> -R binary_read_tests --output-on-failure` |
| 输入 | 无（纯内存构造，见 `tests/importers/spine32/test_binary_reader.cpp`） |
| 输出 | 终端逐用例 PASS/FAIL 与汇总；全通过退出码 0 |
| 依赖 | C++17、CMake、MinGW-w64；复用 `tests/domain/test_framework.hpp`，零第三方测试库 |
| 可安全重跑 | 是（全部为确定性合成输入） |

### 2.8 `tools/binary-read-smoke/` — 真实语料只读扫描（Task 4 B4）

| | |
|---|---|
| 相对路径 | `tools/binary-read-smoke/` |
| 用途 | 用真实 Spine 语料独立校验 Reader：strict EOF + 结构计数 + manifest 锁定样本对照 |
| 使用说明 | [`tools/binary-read-smoke/README.md`](tools/binary-read-smoke/README.md) |
| 运行入口 | `cmake -S . -B <build> -DLOONGBONES_BUILD_BINARY_READ_SMOKE=ON`；`cmake --build <build> --target loongbones_binary_read_smoke`；`<build>/tools/binary-read-smoke/loongbones_binary_read_smoke "$LOONGBONES_CORPUS_ROOT"` |
| 输入 | 语料根目录（**只读**），由 `$LOONGBONES_CORPUS_ROOT` 或首参数提供 |
| 输出 | 终端：Full / Primary 两套分母的成败、失败分类、前 20 条失败明细（相对路径 + 码 + 字节偏移 + 段名）、manifest 16 个锁定样本对照、结论与**退出码** |
| 依赖 | C++17、CMake、MinGW-w64 |
| 可安全重跑 | 是；**只读**，不写任何文件、不复制语料 |
| 退出码 | **只有 0 可作验收证据**。0=全通过；2=无语料根；3=根下无 `.skel.bytes`；**4=锁定样本 0 匹配（语料根多半给错）**；5=对照不一致；6=仍有文件未通过 |
| 当前基线 | **Full 1113/1113、Primary 963/963**，manifest **16 个**锁定样本 **16/16 一致**，退出码 0 |

### 2.9 `tools/spine32-mapper-tests/` — B5 Atlas Reader 与 Mapper 测试

| | |
|---|---|
| 相对路径 | `tools/spine32-mapper-tests/` |
| 用途 | 32 项自动化测试：`.atlas` 文本解析（17）与 Source→canonical 映射（15） |
| 使用说明 | [`tools/spine32-mapper-tests/README.md`](tools/spine32-mapper-tests/README.md) |
| 运行入口 | `cmake --build <build> --target loongbones_spine32_mapper_tests`；`ctest --test-dir <build> -R spine32_mapper_tests --output-on-failure` |
| 输入 | 无（纯内存合成文本与合成 IR）。语料 1113 个单位全是单页图集，多页 / rotate / trim / 页属性只能用合成输入给反例 |
| 输出 | 终端逐用例 PASS/FAIL 与汇总；全通过退出码 0 |
| 依赖 | C++17、CMake、MinGW-w64；复用 `tests/domain/test_framework.hpp`，零第三方测试库 |
| 可安全重跑 | 是（全部为确定性合成输入） |

### 2.10 `tools/spine32-mapper-smoke/` — B5 语料只读扫描

| | |
|---|---|
| 相对路径 | `tools/spine32-mapper-smoke/` |
| 用途 | 对真实语料跑完整链路（Reader → Atlas Reader → Mapper），数出 region 精确命中 / 尾随空格降级 / 未命中 |
| 使用说明 | [`tools/spine32-mapper-smoke/README.md`](tools/spine32-mapper-smoke/README.md) |
| 运行入口 | `cmake -S . -B <build> -DLOONGBONES_BUILD_SPINE32_MAPPER_SMOKE=ON`；`cmake --build <build> --target loongbones_spine32_mapper_smoke`；`<build>/tools/spine32-mapper-smoke/loongbones_spine32_mapper_smoke "$LOONGBONES_CORPUS_ROOT"` |
| 输入 | 语料根目录（**只读**），由 `$LOONGBONES_CORPUS_ROOT` 或首参数提供 |
| 输出 | 单位总数 / Reader 失败 / 图集缺失 / region 附件总数 / 非 region 跳过 / 降级 / 未命中，以及未命中与缺图集单位清单 |
| 依赖 | C++17、CMake、MinGW-w64 |
| 可安全重跑 | 是；**只读**，不写任何文件、不复制语料、不解码像素 |
| 退出码 | **只有 0 可作验收证据**。0=全通过；2=无语料根；3=根下无 `.skel.bytes`；**4=没有任何单位走完映射链路**；6=存在 region 未命中 |
| 当前基线 | 1113 个单位，进入映射 1111（2 个缺图集），**region 附件 27,457 全部命中、未命中 0** |

### 2.11 `tools/skeleton-import-tests/` — B6 宿主事务与插件集成测试

| | |
|---|---|
| 相对路径 | `tools/skeleton-import-tests/` |
| 用途 | 25 项自动化测试：`test_import_host.cpp`（宿主事务 10）+ `test_spine32_plugin.cpp`（插件集成 15） |
| 使用说明 | [`tools/skeleton-import-tests/README.md`](tools/skeleton-import-tests/README.md) |
| 运行入口 | `cmake --build <build> --target loongbones_skeleton_import_tests`；`ctest --test-dir <build> -R skeleton_import_tests --output-on-failure` |
| 输入 | 无（内存合成 `.skel.bytes` 字节 / `.atlas.txt` 文本 / PNG 头） |
| 输出 | 终端逐用例 PASS/FAIL 与汇总；全通过退出码 0 |
| 依赖 | C++17、CMake、MinGW-w64；复用 `tests/domain/test_framework.hpp`，零第三方测试库 |
| 可安全重跑 | 是（全部为确定性合成输入） |

### 2.12 `tools/skeleton-import-smoke/` — B6 最小宿主 + 语料端到端验证

| | |
|---|---|
| 相对路径 | `tools/skeleton-import-smoke/` |
| 用途 | 无 GUI 的最小宿主：在真实语料上跑「注册表 → prepare → 事务提交 → 重开验证」完整闭环 |
| 使用说明 | [`tools/skeleton-import-smoke/README.md`](tools/skeleton-import-smoke/README.md) |
| 运行入口 | `cmake -S . -B <build> -DLOONGBONES_BUILD_SKELETON_IMPORT_SMOKE=ON`；`cmake --build <build> --target loongbones_skeleton_import_smoke`；`<build>/tools/skeleton-import-smoke/loongbones_skeleton_import_smoke "$LOONGBONES_CORPUS_ROOT"` |
| 输入 | 语料根目录（**只读**），由 `$LOONGBONES_CORPUS_ROOT` 或首参数提供 |
| 输出 | prepare 成功 / 失败、完整提交数、落盘资源文件数、region 附件数、降级 / 未命中、不支持能力账本、诊断码分布，以及重开后的对象计数摘要 |
| 依赖 | C++17、CMake、MinGW-w64 |
| 可安全重跑 | 是；语料只读，写入只发生在 `--project` 指定的暂存目录，结束即清理 |
| 退出码 | **只有 0 可作验收证据**。0=端到端闭合通过；2=无语料根；3=根下无 `.skel.bytes`；4=无有效候选；5=提交/重开不一致；6=存在 error 级诊断；7=失败全部属共享图集布局的已知缺口 |
| 当前基线 | 1113 个单位，prepare 成功 1111（2 个共享图集布局缺同名图集），**region 未命中 0**，完整提交 + 重开 8/8 一致 |

### 2.13 `tools/format-tests/` — 原生格式 codec 自动化测试

| | |
|---|---|
| 相对路径 | `tools/format-tests/` |
| 用途 | 70 项自动化测试：47 项在 `src/main.cpp`（codec 往返、严格解析、原子保存），23 项在 `tests/format/test_v2_skeleton.cpp`（v1/v2 双读、迁移、未知字段锚点） |
| 使用说明 | [`tools/format-tests/README.md`](tools/format-tests/README.md) |
| 运行入口 | `cmake --build <build> --target loongbones_format_tests`；`ctest --test-dir <build> -R format_tests --output-on-failure`。可用 `-DLOONGBONES_BUILD_FORMAT_TESTS=OFF` 关闭 |
| 输入 | 无（纯内存构造 + `docs/format/fixtures/` 合成样本） |
| 输出 | 终端 `format-tests: N passed, M failed`；全通过退出码 0 |
| 依赖 | C++17、CMake、MinGW-w64；vendored RapidJSON 仅出现在 `.cpp` |

---

## 3. 文档索引

| 文档 | 内容 |
|---|---|
| [`项目规范.md`](项目规范.md) | 产品与工程规格（负责人权威） |
| [`docs/licenses-and-risks.md`](docs/licenses-and-risks.md) | 许可证与资产风险清单、待裁定问题 |
| [`docs/mapping-spine32-to-native.md`](docs/mapping-spine32-to-native.md) | Spine 3.2.01 → Native Model 字段/语义映射与不确定项（2026-10-20 已补证据修订） |
| [`docs/skeleton/`](docs/skeleton/) | **Task 4 骨架模块设计**：Skeleton Domain 与原生格式兼容提案、插件宿主契约、字段映射表、样本 manifest、合成 fixture、阶段 A 验收报告（含返修记录）、B0 验收报告、B4 Reader 报告 |
| [`docs/format/native-project-v2-skeleton.md`](docs/format/native-project-v2-skeleton.md) | 原生格式 v2（Skeleton 增量）Schema 冻结稿 |
| [`docs/format/native-project-v3-mesh.md`](docs/format/native-project-v3-mesh.md) | 原生格式 v3（第二批骨架类型增量）Schema **提案**（Task 5 A3，待审阅） |
| [`docs/skeleton/mesh-and-constraint-rfc.md`](docs/skeleton/mesh-and-constraint-rfc.md) | 第二批附件与约束 canonical 模型**提案**（Task 5 A2，待审阅） |
| [`Agent_Task/Task4.md`](Agent_Task/Task4.md) | Task 4 Spine 导入插件、宿主 API 与骨架数据契约执行规格 |
| [`Agent_Task/Task4_Back_task.md`](Agent_Task/Task4_Back_task.md) | Task 4 阶段 A 文档契约复核返修规格（已复核通过并关闭） |
| [`docs/adr/`](docs/adr/) | ADR/RFC 草案（Phase 1 待签定） |
| [`PHASE0_REPORT.md`](PHASE0_REPORT.md) | Phase 0 阶段报告 |
| [`docs/scene-system/SCENE_SYSTEM_SPEC.md`](docs/scene-system/SCENE_SYSTEM_SPEC.md) | Scene、唯一 Root、普通节点均可挂载、Group/Sprite 与 Atlas/AtlasRegion 职责 |
| [`Agent_Task/Task2.md`](Agent_Task/Task2.md) | Task 2 场景领域模型与通用节点层级执行规格 |
| [`Agent_Task/Task2_Back_task.md`](Agent_Task/Task2_Back_task.md) | Task 2 独立验收发现项及返修规格 |

---

## 4. 外部资源定位规则

- **语料根**（Sdorica 3.2.01）：本地路径由环境变量 `LOONGBONES_CORPUS_ROOT` 或本地配置提供，
  **不写入本仓库任何文件**。示例用占位变量，不写盘符/用户名（规范 §8.2、§9.3）。
- **Spine Runtime 参照源码**：位于旧工作区 `resources/extern/vendor/spine-r35/` 等处，
  受 Spine Runtimes License v2.5 约束，仅限内部行为参照，**不可移植/分发**（见许可清单）。
- **旧项目资产**：`D:/Works/Spine/converter/`、`runtime/`、`resources/plugin/`。
  Phase 0 仅借用其 Reader 作分析工具；产品代码须独立实现（规范 §1.2）。

---

## 5. 目录位置裁定

按规范 §8.2「源码目录按职责/依赖分层，不按开发者姓名、临时任务号或当前日期组织」
对顶层目录逐项裁定。已按裁定落地的改动记录在同一行。

| 目录 | 裁定 | 依据 / 说明 |
|---|---|---|
| `domain/` `format/` `importers/` `tests/` `docs/` `tools/` | **合理，保持** | 规范 §8.2 建议分区；`importers/` 对应其 `plugins/` 或 `importers/` |
| `vendor/rapidjson/` | **合理，保持** | 规范建议列表未列，但第三方依赖必须与产品源码分离；有 `VENDOR.md` 自证来源 + 许可证全文，已登记 `docs/licenses-and-risks.md` |
| `app/` `runtime/` `render/` `ui/` | **暂不创建** | 规范 §8.2 建议分区，但 §1「在负责人签定前不预先创建空目录、不假定技术栈」 |
| `tests/domain/` `tests/format/` `tests/importers/spine32/` | **合理（本轮已归位）** | 测试源码按被测模块镜像分目录。本轮把 `tools/format-tests/src/test_v2_skeleton.cpp` 移入 `tests/format/`，`tests/importers/test_atlas_import.cpp` 移入 `tests/importers/spine32/`，使「测试源码在 `tests/`、`tools/*-tests/` 只放构建入口与说明」这一约定在全部四个测试目标上一致 |
| `tools/*-tests/` `tools/*-smoke/` | **合理，保持** | 规范 §8.2 明确要求开发验证工具（含测试 runner）放在 `tools/`，并配就近说明 |
| `Agent_Task/` | **待负责人裁定** | 目录名含执行者、文件按任务号命名（`Task2/3/4.md`），与 §8.2「不按开发者姓名、临时任务号组织」冲突；但 `项目规范.md`（负责人权威）自身引用该路径，故**不由 Agent 单方面迁移**。建议改为 `docs/tasks/`，需负责人同步修订规范正文 |
| `build*/`（历史 15 个） | **已移出仓库（2026-10-21）** | §8.2「构建缓存不得进入源码目录」。原 15 个并存目录已整体移至仓库外的 `D:\Works\Spine\.builds\`（与 `.devtools` 同级）。因 `CMakeCache.txt` 绑定绝对路径，这些历史目录**已失效、仅作归档**；当前构建根为 `..\.builds\main`。详见 `Agent_Task/DataType_Report.md` §5.1 |

### 5.1 本轮目录整理改动清单

- 删除根目录 196 个 `.log` / `.err`（构建与测试运行产物，可重建）。
- 新增 `.gitignore`（精确规则：顶层 `build*/`、运行日志、两个工具的生成目录、IDE 文件）。
- 新增 `.editorconfig`（UTF-8 无 BOM、LF、2 空格缩进；既有文件不批量重格式化）。
- `tools/skeleton-probe/vert-probe.mjs`：移除写死的语料绝对路径，改由
  `$LOONGBONES_CORPUS_ROOT` 提供，缺参退出码 2（规范 §8.2）。
- 新增 `tools/binary-read-tests/README.md`（该工具此前缺少就近使用说明）。
- 修 `STRUCTURE.md` 章节错位（原 §3/§4 夹在 §2 工具登记簿中间）。
