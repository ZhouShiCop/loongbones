# 许可证与资产风险清单（Loongbones Phase 0）

对应规范：`Loongbones/项目规范.md` §5 Phase 0、§11 风险表、§12 交付物 5。
**本文件是风险记录，不是法律意见。** 标注 `需负责人裁定` 的条目在签定前不得进入产品代码或发行包。

---

## 1. 逐项清单

### 1.1 Spine Runtime 源码（spine-r35 / spine32_runtime）— **高风险，需负责人裁定**

| 项 | 事实 |
|---|---|
| 位置 | `resources/extern/vendor/spine-r35/`（clone 自 `EsotericSoftware/spine-runtimes`，分支 3.5）；`D:\Game\Other\sdorica\working\spine32_runtime\spine-runtimes-b9bd66b36b4e9d5a7a5bd6b4f65ad371359749b3/`（commit b9bd66b，含 3.2.01 参照文件） |
| 许可证 | `spine-r35/LICENSE` = **Spine Runtimes Software License v2.5**（Esoteric Software） |
| 关键条款 | 授权范围 "solely for **personal or internal use**"；未经书面许可**不得** "(a) modify, **translate**, adapt, or develop new applications using the Spine Runtimes or otherwise create derivative works"；再分发须附带该许可证 |

**影响与处置**：

- ✅ **允许**：作为**行为参照**阅读，用于理解公开可观察的语义（坐标方向、`UpdateUVs` 的数学、继承规则等），并据此写我们自己的实现与测试期望值。
- ❌ **禁止（除非取得 Esoteric Software 书面许可）**：把其源码**移植/翻译**成 C++/任何语言后并入产品；以"Spine 源码的翻译"作为交付物；打包进发行包。
- ⚠️ **灰区，需负责人裁定**：`MeshAttachment.UpdateUVs` 这类**数学公式**的等价重写、`.skel`/`.atlas` **数据格式的解析**是否构成 derivative work。建议按"只实现公开文档化的数据格式 + 自行验证的数值行为"推进，并保留独立实现的证据链（自研 Reader 的 golden 测试）。
- 🔴 **必须纠正的前期建议**：Phase 0 之前曾提出"把 spine-r35 的 C# runtime 对照移植到 C++"。按上述条款，**该方案不可行**，已撤回。产品实现必须独立编写。

### 1.1.1 已定案：导入器与求值器只做独立实现（负责人 2026-10-19 决定）

**决定**：导入器（Spine 3.2.01 → Native Model）以及动画求值，**只做独立实现**，
不以任何形式移植/翻译 Spine Runtime 源码。

**这条决定的实际覆盖范围**：

| 层 | 处置 | 现状 |
|---|---|---|
| `.skel.bytes` / `.atlas.txt` **数据格式解析** | 独立实现 | ✅ **已完成**：`converter/src/read/binary32.ts` + `atlas.ts` 由本团队编写，经 **963 个真实文件 strictEof 全通过**验证 |
| **求值器**（世界变换、蒙皮、deform 应用、约束求解、`UpdateUVs` 等数学） | 独立实现 | ⚠️ 需重写为产品代码；数学依据"公开可观察行为 + 自验 golden 值"，**不得**照抄源码结构 |
| **Spine Runtime 源码**（spine-r35 / spine32_runtime / spine38-web） | 仅内部行为参照，可读不可译 | 隔离，不进产品、不提交、不分发 |

**独立实现的证据链要求**（规范 §7 / §8.4）：
- 每个数值行为都要有 golden 值与容差表，且 golden 值来自**我们自己的验证**，不是抄源码输出。
- 语义判断必须在代码注释里写明依据（"依据什么数据/什么可观察行为得出"），便于复审许可边界。
- 求值器的关键结论已有独立验证先例：UV 的 regionUVs 语义、非加权 mesh 顶点为槽骨骼局部坐标、
  deform 的增量/绝对值分域、骨骼帧值为相对 setup 增量——这些都是本团队用真实数据反查得出，
  不是移植源码。

**仍属灰区、继续按既定纪律执行**：数学公式的等价重写与数据格式解析，
按"只实现公开文档化的数据格式 + 自行验证的数值行为"推进，并保留上述证据链。
若后续要公开分发产品，建议就这一边界取得 Esoteric Software 的书面确认（§3 问题 1 仍开放）。

### 1.2 spine38-web 打包产物 — **来源不明，需负责人裁定**

| 项 | 事实 |
|---|---|
| 位置 | `resources/extern/vendor/spine38-web/SpineViewer.CQVJFyEO.js`（262 KB）、`common.CazL1XCQ.js`（318 KB）；`resources/extern/web/web38/spine38-core.js`（221 KB，由前者切出） |
| 来源 | 从 PRTS（明日方舟 wiki）的 Spine 网页播放器打包产物中提取的官方 Spine 3.8.99 runtime |
| 许可证 | **无许可证文件**；未取得 Esoteric Software 分发授权；再分发性不明 |

**处置**：仅限**本地开发期**作为 3.8 行为的交叉仲裁参照，**不得**进入产品、不得公开分发、不得提交到任何可能公开的仓库。

### 1.3 Sdorica 游戏资产 — **专有，禁止入仓库**

- `*.skel.bytes` / `*.atlas.txt` / `*.png`（1,113 / 1,111 / 1,110 个）均为《万象物语》游戏资源，专有。
- 纪律（规范 §8.2/§9.3）：**只读**；不移动/改写/删除/复制进仓库或构建产物；公开 manifest 只记 SHA-256 与统计量，不记盘符/用户名。
- `resources/extern/assets/corpus/` 下已有的预览 PNG 等同理，属开发期产物，不进发行包。

### 1.4 DragonBonesJS — **MIT，宽松**

- `resources/extern/vendor/DragonBonesJS/`，`package.json` 标注 `license: MIT`。
- 新项目已放弃 DragonBones 目标（规范 §1.1.1），此项仅对旧项目有意义；保留作历史参照，不并入新产品。

### 1.5 RapidJSON — **MIT，宽松**（负责人 2026-10-20 决定引入）

| 项 | 事实 |
|---|---|
| 项目 | `Tencent/rapidjson` |
| 版本固定 | release **`v1.1.0`**，commit **`f54b0e47a08782a6131cc3d60f94d038fa6e0a51`**（发布于 2016-08-25） |
| 许可证 | **MIT**（BSD 风格，宽松）。允许商用、修改、再分发；**唯一义务**：保留版权声明与许可声明 |
| 用途 | 原生项目格式 v1（[`docs/format/native-project-v1-rfc.md`](format/native-project-v1-rfc.md)）的 JSON 解析 |
| 引入决定 | 负责人 2026-10-20（RFC §7.3 / 待决项 U1），登记见 [`DECISIONS.md`](../DECISIONS.md) D16 |
| vendored 位置 | `vendor/rapidjson/`（自证文件：[`VENDOR.md`](../vendor/rapidjson/VENDOR.md)） |
| vendored 日期 | 2026-10-20（阶段 B 启动） |
| 取出内容 | `include/rapidjson/` **33 个头文件** + `license.txt`→`LICENSE-MIT.txt`（5152 字节） |
| 版本自证 | `rapidjson.h` 中 `RAPIDJSON_MAJOR/MINOR/PATCH_VERSION` = **1 / 1 / 0** |

**义务（必须履行）**：

1. vendor 入库时**同步 MIT 许可证全文**（上游 `license.txt`）到 vendor 目录，随代码分发。
2. **固定 commit**：`f54b0e47a08782a6131cc3d60f94d038fa6e0a51`。**不接受**直接跟随上游 `master`；升级须走与引入时相同的评审与登记流程。

**隔离约束**（防止许可证与依赖方向问题扩散）：

- RapidJSON 只出现在 `format/`（及阶段 B 的 `tools/format-tests/`）的**实现文件（`.cpp`）**中；
- **不得**出现在任何**公开头文件**；
- **不得**进入 `domain/`（规范：禁止 Domain 依赖 JSON 库）。

**用法**：不直接使用 RapidJSON 的 DOM；用其 **SAX 接口**自建 JSON 值树，以满足「重复 key 必须拒绝」与「未知数字逐字节保留原始字面量」两项格式硬要求（RFC §7.3、§6.6）。

**第三方组件的处置（2026-10-20 vendor 时完成，vendored 树内 100% 为 MIT）**：

| 组件 | 上游许可证 | 处置 |
|---|---|---|
| `bin/jsonchecker/` | **JSON License**（含 "shall be used for Good, not Evil" 条款，与宽松许可冲突） | **未取出**（只取 `include/rapidjson/`） |
| `include/rapidjson/msinttypes/` | **BSD-3-Clause** | **已删除**。依据：`rapidjson.h:162` 的包含条件为 `#if defined(_MSC_VER) && (_MSC_VER < 1800)`（VS2013 之前），与本项目 **C++17** 要求互斥，该分支不可达 |

**状态**：**已 vendor 入库（2026-10-20），阶段 B 使用中。** 义务 1、2 已履行。

### 1.6 旧项目自研代码 — 本项目自有

- `converter/`、`runtime/`、`resources/plugin/` 由本团队编写，是本项目的既有资产。
- 复用时须按规范 §1.2 审查：**不得把旧项目的 DragonBones 目标或实现直接当新产品架构**；数据层（Reader/atlas/IR）可作为独立实现证据与 golden 值来源。

---

## 2. 风险登记（Phase 0 增量）

| # | 风险 | 影响 | 处置 | 状态 |
|---|---|---|---|---|
| R1 | Spine Runtime 许可证禁止 translate/derivative works | 若照搬源码，产品有侵权风险 | **已定案：只做独立实现**（负责人 2026-10-19 决定）。详见 §1.1 | **已定案** |
| R2 | spine38-web bundle 无许可证 | 无法公开分发 | 仅本地参照，隔离在仓库外或不提交 | **需负责人裁定** |
| R3 | 游戏资产混入仓库/发行包 | 专有资产泄露 | 只读 + manifest 仅记 hash；`.gitignore` 精确规则 | 已定纪律 |
| R4 | 语料能力覆盖偏窄（无 boundingbox/clipping/path/point 附件、无 path 约束、事件仅 1 个） | 这些能力**无法用真实样本验证**，只能靠合成 fixture | Phase 1 能力矩阵标 `待验证`；合成 fixture 须自制且可再分发 | 新发现 |
| R5 | 963/1113 口径曾被混用 | 回归分母不可靠 | 已复现并固定两种口径（见 Phase 0 报告 §3） | 已解决 |
| R6 | Qt 绑定/语言/渲染后端未定 | 阻塞 Phase 1 垂直切片 | ADR-0001 草案，需负责人签定 | 待决 |
| R7 | 原生格式容器（JSON/二进制/压缩）未定 | 阻塞 schema RFC | ADR-0002 草案，需负责人签定 | 待决 |
| R8 | 引入第三方依赖 RapidJSON（MIT） | 许可证义务与依赖扩散 | 固定 commit `f54b0e47…` + 同步 MIT 全文 + 仅出现在 `format/` 实现文件、不进公开头文件与 `domain/` | **已决定引入，待阶段 B vendor 入库** |

---

## 3. 需要负责人裁定的问题

1. ~~是否就"独立实现 + 公开行为参照"路线取得 Esoteric Software 书面确认？~~
   **已定案：只做独立实现，不走移植路线。** 剩余问题仅剩：若产品要**公开分发**，
   是否需就"数学公式等价重写 + 数据格式解析"的边界取得书面确认。
2. `resources/extern/vendor/spine38-web/` 与 `resources/extern/web/web38/` 是否从工作区移出或明确标记为"不可分发"？
3. 产品是否可能公开分发？若是，R1/R2 的边界需在写第一行产品代码前确定。
4. 语料无法覆盖的能力（R4），由谁提供合法可再分发的合成样本？
