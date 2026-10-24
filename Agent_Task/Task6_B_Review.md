# Task 6 阶段 B 验收复核

- 复核对象：Task 6 阶段 B（B1 接口 / B2 匹配与诊断 / B3 测试与语料对账）
- 交付报告：`Agent_Task/Task6_B_Report.md`
- 依据：`Agent_Task/Task6_Implementation_Plan.md` §7（工作包 + 6 条断言 + 语料口径）、**D40 / D53 / D54**
- 复核日期：2026-10-24
- 角色边界：本文件只作**验收**；本轮未改产品代码

---

## 0. 结论

**判定：通过（2026-10-24 更新）。**

> 首轮判定为**不通过**（1 个 P1：D53 未落地）。负责人当日裁定 **「A」** ⇒ 落 **D55**：
> D53 的实现推迟到 **Task 7 / v4**（v3 已冻结、域模型缺「源文件身份」字段），
> Task 6 **明写已知限制**即可。⇒ 阻断项解除，**阶段 B 通过**；
> 剩余 3 项**非阻断返修**（P1-2 测试命名/断言、P2-1 去兼容层、P2-2 报告状态）须在 Task 7 开工的同一批清掉。

B1 / B2 的匹配逻辑、B3 的 6 个单元测试、以及**语料端到端退出码 0** 全部达标，且经本对话**独立复跑**与报告数字完全一致。
**但 D53（共享图集共用同一 Atlas / TextureAsset 对象）未落地** —— 报告已如实披露（§5），
而这条是阶段 B 的契约之一（计划 §7 B2）。

**根因不只是实现遗漏**：D53 需要「**源文件身份**」才能判定"同一份源"，而
`Atlas` / `TextureAsset` **都没有源路径字段**（`atlas.hpp:54-63`、`:106-111`），
v3 schema 已在 Task 5 冻结、v4 归 Task 7（D46）⇒ **这是我的计划层未识别的依赖**，
需负责人裁定落地时机（§7.1）。

---

## 1. 交付核对

| 包 | 报告声称 | 复核 |
|---|---|---|
| **B1** 接口 + 插件侧实现 | `IResourceResolver::listDirectory()`（**带默认实现返回 false**）+ `FileSystemResourceResolver` 覆盖 | ✅ 代码在位；安全校验与 `read()` 同款（见 §3） |
| **B2** 匹配与诊断 | 同名优先 → 目录内唯一候选 → ≥2 歧义拒绝 → 0/不可用维持缺失；新增 `kResourceAmbiguous` | ✅ 顺序与 D54 一致 |
| **B3** 单测 | 6 个新测试，套件 40 → 46 | ✅ 实测 46/46；6 个测试确实存在 |
| 语料闭环 | prepare 1113/1113、失败 0、exit 0 | ✅ 独立复跑一致 |
| **D53 资产共用** | **未实现、未验证**（报告 §5 自述） | ⚠️ **已裁定延后（D55 → Task 7 / v4）**；Task 6 须明写已知限制（§4、§7.1） |

---

## 2. 本对话独立复跑（不采信转述）

在 `D:\Works\Spine\.builds\build_t6`（MinGW Makefiles，`g++.exe`，smoke ON）执行：

| 项 | 报告 | 本对话实测 | 判定 |
|---|---|---|---|
| `mingw32-make -j8` | exit 0，0 error / 0 warning | **exit 0**，error/warning 行 **0** | ✅ |
| `ctest` | 7/7，exit 0 | **7/7，exit 0** | ✅ |
| `loongbones_core_tests` | 135 | **135 / 135 pass** | ✅ |
| `loongbones_format_tests` | 86 | **86 pass** | ✅ |
| `loongbones_binary_read_tests` | 47 | **47 / 47** | ✅ |
| `loongbones_spine32_mapper_tests` | 48 | **48 / 48** | ✅ |
| `loongbones_skeleton_import_tests` | 46 | **46 / 46** | ✅ |
| `loongbones_atlas_import_tests` | 9 | **9 / 9** | ✅ |
| `binary-read-smoke` | exit 0；Full 1113/1113、Primary 963/963 | **exit 0**；Full **1113 ok / 0 fail**、Primary **963 ok / 0 fail**、manifest 16/16 | ✅ |
| `skeleton-import-smoke` | exit 0；prepare 1113、失败 0、8/8、region 0、deep-compare 0 | **exit 0**；**1113 / 0**、8/8、region 未命中 **0**、`[deep-compare] items=18067 maxResidual=0 mismatches=0`、`prepare 失败中共享图集布局 0` | ✅ |
| 诊断汇总 | 无 `IMPORT_RESOURCE_MISSING` / `_AMBIGUOUS` | ✅ 实测仅 `ATTACHMENT_REGION_DEGRADED 4` + `UNSUPPORTED_CAPABILITY 6646` | ✅ |

> 结论：**报告的数字全部属实**，没有包装、没有口径漂移。

---

## 3. 判据逐条核对（计划 §7.1 的 6 条）

| # | 断言 | 判定 | 依据 |
|---|---|---|---|
| 1 | 目录内唯一候选 ⇒ 命中并成功导入（两骨架都指向**同一** Atlas） | ⚠️ **半条** | `SharedAtlasResolvesBothSkeletons` 只断言"两次 prepare 各自 1 图集 / 1 纹理、区域数相等"——**这是单次导入的必然结果**，**没有**断言 `Atlas.id` 相同（§4.2） |
| 2 | 歧义 ⇒ **显式拒绝**、诊断含候选数、不得静默成功 | ✅ | `AmbiguousAtlasCandidatesAreRejected`：`!draft.valid` + `kResourceAmbiguous` + **断言不得落成** `kResourceMissing`；fixture 18 的两个图集都含目标 region ⇒ "取第一个"也能成功，故该断言**真有鉴别力** |
| 3 | 多图集目录内**同名优先**（不判歧义、不取 decoy） | ✅ | `SameNameAtlasWinsOverOtherCandidate`：`draft.valid` + 无 `kResourceAmbiguous` + 无 `kUnresolvedReference`（decoy 无目标 region ⇒ 误取必然暴露） |
| 4 | 列举安全边界：`..` / 超出授权根 / 目录不可读 | ✅ | `ListingOutsideAuthorizedRootIsRejected`（真实 `FileSystemResourceResolver`：`..`、`sub/../../..` 均被拒；合法列举只回一层普通文件名）、`ListingMissingDirectoryIsRejected` |
| 5 | 既有 1111 个同名单位解析结果不变 | ✅ | 代码在"同名 read 成功"时**提前返回**，不进入列举（`spine32_import_plugin.cpp:229`）；语料 1113 全成功、deep-compare 0 差异 |
| 6 | 共享资源计数：`atlases=1` / `textures=1`、同一 `Atlas.id`、不重复暂存 | ❌ **未达成** | 见 §4 |

### 3.1 B1 实现质量（读过代码，非只看报告）

`FileSystemResourceResolver::listDirectory`（`atlas_import.cpp:572-620`）：

- ✅ `isSafeRelative` → `canonical` → `pathWithin` 三道与 `read()` **同款**，符号链接逃逸会被 canonical 解析后拒绝；
- ✅ 只收 `is_regular_file`（不递归、不含子目录、不含路径），`std::sort` **确定性排序**；
- ✅ 失败分类沿用 B6 三种（目录不存在 / 无法解析 / 越出授权根），"目录不存在"与"无法解析"**分开报**（与 `read()` 一致）；
- ✅ 依赖方向未变（接口在 `importers/api`，文件系统实现在插件侧）。

---

## 4. P1：D53 未落地（共享图集仍会各建一份资产）

### 4.1 代码事实

| 位置 | 行为 |
|---|---|
| `spine32_import_plugin.cpp:276-278` | 每次 prepare 都 `atlas.id = ids.create(IdKind::Atlas)`，**新 ID** |
| `spine32_import_plugin.cpp:420-422` | `candidate = request.current.data` 后**无条件** `candidate.atlases.push_back(atlas)`、`candidate.textures.push_back(...)` |
| `spine32_import_plugin.cpp:309-311` | `texture.id = ids.create(...)`；`texture.location = "assets/textures/<新 id>.png"` ⇒ **同一张 PNG 会被写成两个不同文件** |
| `spine32_import_plugin.cpp:311` | 纹理**名字**会去重（`h0164s5_watch` → `h0164s5_watch_1`），但**对象与文件仍然重复** |

⇒ 把 `h0164s5_watch_hour` 与 `h0164s5_watch_minute` 导进**同一工程**的结果是：
**2 个 Atlas（各自新 ID，都指向同一 `h0164s5_watch.atlas.txt`）+ 2 个 TextureAsset（同一 PNG 两份字节）**。
与 **D53**（同一份源文件 ⇒ 一个 `Atlas.id` / 一个 `TextureAsset.id`、`textures` / `atlases` 各 1 份）**直接冲突**。

### 4.2 证据侧还有两处问题

1. **测试名与断言不匹配**：`SharedAtlasResolvesBothSkeletons` 读起来像"两骨架共用同一 Atlas"，
   实际只断言两次 prepare 的**各自计数**——即使 D53 完全没实现，它也会通过。**属"名义覆盖"**。
2. **验收工具把违反 D53 的行为写死了**：`tools/skeleton-import-smoke/src/main.cpp:446-449`
   ```cpp
   reopened.document.data.atlases.size() == t.committed      // 1 单位 = 1 图集
   reopened.document.data.textures.size() == t.textures
   reopened.document.data.atlases.size() == t.committed      // 同一条件写了两遍
   ```
   ⇒ 如果**正确**实现 D53（两单位共享 1 图集），这条断言会**判失败**。
   即：修 D53 必须同步改造 smoke 的断言（按"去重后的源图集数"而非"单位数"核对）。

### 4.3 根因（不把锅全甩给实现会话）

D53 需要判定"**同一份源文件**"，而域模型里 **没有源路径字段**：

- `TextureAsset` 只有 `id / name / storage / location / pixel*`（`atlas.hpp:54-63`），`location` 是**工程内**路径（`assets/textures/<id>.png`），不是源路径；
- `Atlas` 只有 `id / name / pages / regions`（`atlas.hpp:106-111`），`name` 现在存的是**骨架 stem**。

**按 `name` 去重的方案已排除**：语料里 **33 个图集 stem 跨目录重名**（例：`b0037s3` 出现在 4 个目录）
⇒ 名称去重会**误合并不同目录的图集**（比重复更糟）。

⇒ 要么加"源路径"字段（**动 v3 schema**，而 v3 已在 Task 5 冻结、v4 归 Task 7 / D46），
要么明确把 D53 推到 Task 7。**这是决策问题，不是实现细节** —— 我的计划 §7 未识别这条依赖，已记录为本轮教训。

---

## 5. P2 / P3

| # | 级别 | 内容 |
|---|---|---|
| **P2-1** | 设计 | `listDirectory` 给了**默认实现（返回 false）**：这是**兼容层**（与负责人「不为兼容绕道」的取向相悖），且带来**静默降级** —— 任何未覆盖它的 resolver 会让共享图集悄悄退回"资源缺失"，编译期无提示。当前只有插件侧 `FileSystemResourceResolver` 覆盖了它，3 个测试 `FakeResolver` 中仅 1 个覆盖。建议改**纯虚**（3 个 Fake 各补几行即可），或加**能力查询 + 独立诊断**，不要静默 |
| **P2-2** | 文档 | 报告**自相矛盾**：状态行（第 6 行）与停点（§6）仍写「B3 单测与 A4 fixture 未做」，而 §5（第 109-110 行）已写「已补 6 个 / 已由阶段 A 补齐」。属状态未刷新，需改 |
| **P2-3** | 环境 | 仓库根多出 **`build_t6/`**（782K，Ninja 配置残留，应为首次 `CXX` 环境变量事故的产物）。虽被 `.gitignore:7 /build*/` 忽略、不会提交，但**违反「构建产物一律放仓库外」**（应只在 `D:\Works\Spine\.builds\`）。建议删除并复核 |
| **P3-1** | 可定位性 | 歧义诊断只报**候选数**（`"候选数 2"`），未列候选文件名。RFC 要求"可定位"；建议附候选名（上限几个），排查更快 |
| **P3-2** | 整洁 | smoke 447 / 449 是**同一个条件写了两遍**，改 D53 断言时顺手去重 |

---

## 6. 返修清单

| # | 级别 | 内容 | 落点 |
|---|---|---|---|
| **P1-1** | **必做（待裁定后）** | 按 §7.1 的裁定实现 D53 或正式改期；实现时须按"**去重后的源图集数**"改 smoke 断言（`atlases.size() == committed` 两处） | `spine32_import_plugin.cpp` / `skeleton-import-smoke/src/main.cpp` |
| **P1-2** | **必做** | 把 `SharedAtlasResolvesBothSkeletons` 改名或补断言：要么**真断言** `left.document.data.atlases[0].id == right...id`（D53 后），要么明确改名为 `SharedAtlasPreparesBothSkeletons`（仅证明可解析） | `tests/importers/test_spine32_plugin.cpp:836` |
| **P2-1** | 应做 | `listDirectory` 去兼容层（纯虚或能力查询 + 独立诊断） | `import_plugin.hpp:99-117` |
| **P2-2** | 应做 | 报告状态行 / 停点与 §5 对齐（B3 已完成、A4 已由阶段 A 交付） | `Task6_B_Report.md` |
| **P2-3** | 应做 | 删除仓库内 `build_t6/`，构建只在 `.builds/` 下 | 仓库根 |
| **P3-1** | 可选 | 歧义诊断附候选名（截断） | 同上 |
| **P3-2** | 可选 | smoke 重复条件去重 | 同上 |

---

## 7. 待负责人裁定

### 7.1 D53 的落地时机 —— ✅ **已裁定：选项 A（2026-10-24）**

| 选项 | 内容 | 代价 | 我的建议 |
|---|---|---|---|
| **A** | **推到 Task 7（v4）**：v4 给 `Atlas` / `TextureAsset` 加「**源路径**」字段，届时实现跨单位复用；Task 6 在支持矩阵明确记「2 个共享单位同工程导入会各建一份资产（已知限制）」 | 现在留一个已知限制；但不动冻结的 v3 | ✅ **已采纳（D55）** |
| **B** | **现在动 v3**：追加密源路径字段 | 重开 Task 5 已冻结的 v3 codec（**重复键陷阱**：解码侧已知键必须同步）、可能要提前 v4 | 不推荐 |
| **C** | 用 `Atlas.name` 存**授权根相对路径**做去重键（零 schema） | `name` 从人类可读名变成路径（UI 显示难看）；语义混杂 | 可行但不优雅 |

### 7.2 P2-1（去兼容层）是否本轮做 —— ✅ **本轮做（随 Task 7 开工同批）**

- 纯虚化 + 3 个 `FakeResolver` 补实现（各几行），否则"静默降级"会留到下一个任务。

---

## 8. 停点

- **阶段 B 通过（2026-10-24，D55 裁定后）**；Task 6 **可收尾归档**（在 Task 7 开工同批清掉 3 项非阻断返修）。
- 达成项明确：**B1 / B2 匹配 / B3 六测 / 语料 `1113/1113、失败 0、smoke 退出码 0`** 均已完成并经独立复跑证实。
- 返修后本对话二次复核（重点：D53 的落地方式与 smoke 断言改造、`SharedAtlas*` 断言是否真有鉴别力）。
- 工作区改动（含 Task 5 叠加栈）仍**全部未提交**，由负责人手动提交。
