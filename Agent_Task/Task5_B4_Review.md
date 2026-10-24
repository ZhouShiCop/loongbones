# Task 5 阶段 B：B4 复核（插件诊断与报告，第一轮）

- 上游：`Agent_Task/Task5_B4_Report.md`（实现报告，第一轮）、`Agent_Task/Task5_Implementation_Plan.md` §4 B4 与 §5 测试矩阵
- 日期：2026-10-22
- 复核人：验收会话（本对话，只检查/验收/计划，不实现）
- 复核结论：**第一轮不通过（2 个 P0 级验收缺口 + 3 个观察项）**；**二次复核仍不通过**（P0-1 / P1 / P2 已闭合，P0-2 的 4 个拒绝路径未闭合，ctest 6/7）；**三次复核通过**（根因定位为 `Status::ok()` 静态工厂陷阱这一**真实产品缺陷**，已修复；ctest 7/7、skeleton_import 40/40）。详见 §9 / §10。

---

## 1. 复核方法

1. **静态核对 git 工作区改动**（不重跑 20 分钟全量构建）：`git diff --stat` 显示 32 文件改动、约 2959  insertions；B4 相关改动集中在 `importers/spine32/src/spine32_import_plugin.cpp`（+44）、`tests/importers/test_spine32_plugin.cpp`（+42）、`tests/importers/spine32/test_mapper.cpp`（+336，属 B3 栈）等。
2. **逐项核对测试覆盖**：对 `test_spine32_plugin.cpp` 做 `git diff` + `grep -c "TEST("`，对照计划 §5 下限；对 `test_import_host.cpp` 确认是否落入改动（B6 宿主断言零回归）。
3. **代码审查**：通读 `spine32_import_plugin.cpp` 新增的 `domainDiagnosticToImportCode()`、账本注释、`adoptData()` 失败路径的消息改写，对照计划 §4 B4 与 RFC 的 9 个新域码分类。
4. **未独立复跑构建**：实现会话的 `build_b4` 已知 `mingw32-make` 退出码 2 异常（§6 P2）+ `c++.exe` 消失导致历史缓存失效，重跑需全新干净目录 + 全量重编。P0 判据（断言数量、端到端覆盖）属**源码级事实**，无需构建即可确认；构建健康度作为观察项单列。

---

## 2. 验证结果（对照报告核实，构建数字取自实现报告）

| 项 | 来源 / 对象 | 结果 |
|---|---|---|
| `test_spine32_plugin.cpp` 测试数 | `grep -c "TEST("` | **18**（基线 17 → **+1**） |
| B4 新增 TEST 内容 | `git diff` | 仅 `DomainDiagnosticCodesMapToStableImportCodes`（**14 条 CHECK**，纯映射函数单测） |
| `test_import_host.cpp` 是否改动 | `git diff --stat` | **未列入** ⇒ B6 宿主断言**零回归**（静态确认） |
| 插件映射实现 | `spine32_import_plugin.cpp` | `domainDiagnosticToImportCode()` 已落地，分类与计划一致（见 §3） |
| `adoptData()` 失败路径 | 同上 | 现把域码名写进消息（`候选工程未通过 Domain 校验[CODE]: …`）并按映射选 ImportCode |
| 账本注释 | 同上 | `boundingBox` 仍记"待验证"；`ikConstraints`/`transformConstraints` 已支持计数恒 0 ⇒ 不出现 |
| 事务语义 / D25 / D28 | 代码审查 | 未动（`prepare()`/`commitImport()` 顺序、回滚、授权根、重复导入新 ID 副本、无迁移钩子） |
| ctest / binary / mapper / skeleton_import | 取自报告（未独立复跑） | 7/7、47/47、48/48、28/28（B6 基线 27 → +1） |

> 零回归（宿主侧）已静态确认：`test_import_host.cpp` 未改动，B6 的宿主断言完整保留。

---

## 3. 核心发现 P0-1：`skeleton_import_tests` 新增断言仅 1 个，远低于计划下限 ≥10

计划 §5 测试矩阵原文：

> `loongbones_skeleton_import_tests` ｜ 现状断言 27 ｜ **本阶段新增（建议下限）≥ 10**（端到端、账本、拒绝）｜ fixtures、B6 基线

实测（源码级核实）：`test_spine32_plugin.cpp` 本轮仅新增 **1 个 TEST**（`DomainDiagnosticCodesMapToStableImportCodes`，14 条 CHECK），正是报告 §4 自认的"+1"。**缺口 = 9 个测试**。

更严重的是**性质**而非仅数量：这 1 个测试是映射函数 `domainDiagnosticToImportCode()` 的**纯单测**（给定 `DiagnosticCode` 断言返回 `ImportCode`），并不覆盖计划要求的"端到端、账本、拒绝"三类集成断言。这与 B1/B2 首轮、B3 首轮是**同一类风险**——产品代码写完、回归全绿，但把验收断言后置，使第二批插件行为缺乏自动化钉死。

---

## 4. 核心发现 P0-2：插件层真实 fixture 端到端**未做**——三个关键验收点均无最新证据

计划 §4 B4 验收判据原文：

> 第二批能力不再出现在"不支持"账本（boundingbox 除外，D32 仍记"待验证"）；B6 的 27 项宿主/插件断言零回归。

结合你给的额外信息，插件层端到端要钉死的三个点是：**第二批附件进 canonical、账本无第二批条目、拒绝路径经插件报出域码**。逐点核实：

| 验收点 | 是否有最新证据 | 说明 |
|---|---|---|
| 第二批附件进 canonical | ❌ 无 | 无任何插件层测试用真实 `.skel.bytes + .atlas.txt + PNG` 三件套驱动第二批附件经 `prepare()` 进 canonical 并断言。 |
| 账本无第二批条目 | ⚠️ 继承自 B3，B4 未新增验证 | B3 已使 mapper 不再把 mesh/weightedMesh/… 计入 `unsupported`，插件账本自然继承为空；但 B4 **没有**新增一条插件层断言 `report.unsupported` 不含第二批键，无法证明"B4 这一轮"保持账本干净。 |
| 拒绝路径经插件报出域码 | ❌ 无（仅单测映射函数） | 单测只验了"函数输入域码→输出码"；**没有**测试驱动一个真实第二批拒绝（如 `MESH_WEIGHT_SUM_INVALID`）穿过 `prepare()` 的 `adoptData()` 失败分支，并断言 `draft.report.diagnostics` 含 `kParseError`/`kUnresolvedReference` 且消息带 `[CODE]`。即新写的失败路径（`addDiag(domainDiagnosticToImportCode(diag.code), …)`）**未被集成测试触达**。 |

根成本（报告 §4 自认）：凑齐真实三件套（`.skel.bytes + .atlas.txt + PNG`）有成本。这是已知成本，但仍是**缺失的验收工件**，不能因"成本高"而豁免——计划 §4 把端到端列为 B4 的 headline 验收判据。

> 附带正面：B4 的账本注释显式把 `boundingBox` 保持为"待验证"记录，正好闭合了 `Task5_B3_Review.md` §9.6 提出的开放疑问（"unsupported 账本未含 boundingbox，与其占位不支持定位需确认"）。这一处理与 D32 一致，记为正向。

---

## 5. 观察项 P1 / P2 / P3（不单独构成不通过，但必须记录并闭环）

- **P1 — smoke 未在 build_b4 重跑**：`c++.exe` 消失使 `build_b1b2_smoke` 的 `CMakeCache.txt` 缓存失效，旧结论（第二批能力消失、完整提交 8/8、退出码 7 = 已知共享图集缺口）**仍有效但非最新证据**。按纪律，smoke 退出码只有 **0** 可作验收证据，退出码 7 必须单独列出为已知缺口、不得包装成通过。B4 返修时应以 `-DLOONGBONES_BUILD_SKELETON_IMPORT_SMOKE=ON` 在 `build_b4`（或新干净目录）独立复跑。
- **P2 — `mingw32-make` 退出码 2 与日志不一致**：日志显示全部目标 `Built`、无 error/warning 行，但退出码为 2，原因未查明。按纪律**不当作通过**；ctest 7/7 全绿仅说明产物可用，不替构建健康背书。建议返修时复跑并抓全量日志，确认是否为 install/post-build 步骤或某目标非零返回。
- **P3 — B5 未开工**：双口径闭环（Full 1113 / Primary 963）、≥3 个真实样本 `Spine→Domain→Native→reopen` 逐项比对、失败原子性均未做。按阶段 B 停点纪律，B4 不通过即不授权 B5 开工。

---

## 6. 已确认达成（不应被 P0 掩盖）

- 产品代码完成、编译零 error/warning（报告称）、回归全绿（报告称，未独立复跑）。
- 映射函数实现**正确**且与 RFC/计划一致：引用类 3 码 → `kUnresolvedReference`；取值类 6 码 → `kParseError`；既有 `kRegionNotFound`/`kPageNotFound`/`kTextureAssetNotFound` → `kUnresolvedReference`、`kSkeletonValueNotFinite`/`kTransformNotFinite` → `kParseError`；`kNone` → `kNone`；其余 default → `kDomainRejected`。该映射**有 14 CHECK 单测锁定**，不会因后续重构悄悄漂移。
- 事务语义（B6）/ D25 / D28 一律未动；`test_import_host.cpp` 未改 ⇒ B6 宿主断言零回归（静态确认）。
- 账本注释显式处理 `boundingBox` 仍记"待验证"、ik/transform 计数恒 0，与 D32 一致，闭合 B3 复核的开放疑问。

---

## 7. 返修清单（交给实现会话）

1. **P0-1**：`skeleton_import_tests` 新增 **≥ 10** 个测试，覆盖计划 §5 要求的三类：
   - **端到端**：用真实（或最小合成但结构合法的）三件套驱动第二批附件经 `prepare()` 进 canonical，断言 canonical 中第二批对象计数/字段。
   - **账本**：插件层断言 `draft.report.unsupported` **不含** `mesh`/`weightedMesh`/`weightedLinkedMesh`/`ikConstraints`/`transformConstraints`（boundingBox 仍应在）。
   - **拒绝**：构造一个会触发第二批域码（如 `MESH_WEIGHT_SUM_INVALID` / `MESH_LINKED_PARENT_UNRESOLVED`）的源，断言 `draft.report.diagnostics` 含 `kParseError`/`kUnresolvedReference` 且消息带 `[CODE]`（即新写的 `adoptData()` 失败分支被集成测试触达）。
2. **P0-2**：用真实 `.skel.bytes + .atlas.txt + PNG` 三件套跑插件端到端，钉死 §4 三个点。
3. **P1**：在 `build_b4`（或新干净目录）重跑 `skeleton-import-smoke`，退出码口径：0 或明确 exit 7 = 共享图集缺口。
4. **P2**：复跑构建并抓取全量日志，查明 `mingw32-make` 退出码 2 根因，使退出码回到 0。

---

## 8. 停点与风险提示

- **停点**：返修前不得开工 B5；返修完成后本对话二次复核；**通过也不自动授权 B5**。
- **风险提示**：当前工作区是 **B1/B2 返修 + B3 + B4 的叠加栈，全部未提交**（最近提交 `049b1d3` 是 B 开工批准）。B4 建立在前序工作包之上，返修应与既有未提交改动共同管理。
- **数字纪律**：Full 1113 / Primary 963 双口径不得混用；smoke 退出码只有 0 可作验收证据，已知缺口（共享图集 2 个单位，Task 6）须单独列出，不得包装成通过。

---

## 9. 二次复核（返修后，2026-10-23 凌晨）

### 9.1 返修达成情况（对照第一轮清单）

| 条目 | 状态 | 复核核实 |
|---|---|---|
| **P0-1** `skeleton_import_tests` 新增 ≥ 10 | ✅ | `test_spine32_plugin.cpp` 的 `TEST()` 数 **18 → 30**，新增 **12**，覆盖端到端 / 账本 / 拒绝三类。注：报告写「17 → 29」，实测文件为 **18 → 30**（`EndToEndCommitAndReopenMatch` 未被计入其基线）；属计数口径偏差，**不影响「+12 ≥ 10」的结论** |
| **P0-2** 真实三件套插件端到端 | ⚠️ **部分** | 8 个正向 / 账本端到端 ✅；**4 个拒绝路径（09/10/12/13）❌ 保留为红** |
| **P1** smoke 在 build_b4 重跑 | ✅ | `build_b4` 含 `-DLOONGBONES_BUILD_SKELETON_IMPORT_SMOKE=ON` 重跑：完整提交 8/8、region 未命中 0；退出码 **7** = 已知共享图集缺口，**未包装成通过** |
| **P2** `mingw32-make` 退出码 2 根因 | ✅ | 退出码回到 **0**；根因 = 首次配置未启用 smoke 目标 + 缓存不完整（非编译错误，原日志本无 error/warning 行） |

实现会话把 4 个未闭合测试**保留为红**（不删、不改断言、不放宽）—— 处理正确，予以确认。

### 9.2 结论：二次复核**不通过**

理由：`ctest 6 / 7`，`skeleton_import_tests` 失败（正是 09/10/12/13 四项）。按纪律，测试套件存在失败即不得判通过；P0-2 仍属部分未闭合。

### 9.3 复核中发现的定位问题：mapper 层的「被拒」**不构成**插件层的对照

这是本轮最有价值的发现，直接修正下一轮的方向。`test_mapper.cpp:512-524` 的 `domainRejectCode()`：

- 用 `NameRegions regions({})` —— **空** region 查找（⇒ 所有附件 regionId 均未解析）；
- `ProjectData d` 只 `push_back(r.skeleton)` —— candidate **只含骨架**，无图集 / 无纹理；
- 再 `p.adoptData(d)`。

而插件层 `prepare()` 是：candidate = `current.data` + **textures** + **atlas** + `mapped.skeleton`，region 查找是**真图集** `AtlasRegionIndex({atlas})`。

⇒ 两者**输入并不同口径**（region 是否解析、candidate 是否含图集）。因此报告 §3 由该对照得出的结论「差异不在 Reader、不在 Domain 校验、不在 adoptData 本身」**并未被证明** —— 它不是同口径对照，不能据此排除 Domain 侧。

### 9.4 一个可判定的推论（显著缩小范围）

Domain 的 `checkAttachment`（`domain/src/skeleton.cpp:341-364`）对第二批附件**必然**执行下列判据之一：

- `MeshAttachment` → `checkMeshGeometry(uvs, triangles, vertices.size()/2, …)`：三角索引 ≥ 顶点数 ⇒ `kMeshTriangleIndexOutOfRange`；随后 `checkRegionRef` ⇒ region 未解析则 `kRegionNotFound`。
- `WeightedMeshAttachment` → `checkWeightedMesh`：权重和 / 骨骼闭合 + `checkRegionRef`。

而 fixture golden（README §2 第 73 行）确认 09 为「3 顶点、索引 9」，必拒。

**推论：只要第二批附件真的进了被校验的 skeleton，Domain 就必然拒绝**（几何码与 region 未命中码至少命中一条）。现在插件层 `adoptData()` 返回 ok ⇒ 最可能是 **这 4 个 fixture 的第二批附件在插件路径下没有真正进入 candidate skeleton**（或几何字段为空）。

⇒ 定位方向应从「Domain 为何放行」改为「**映射 / 读取是否产出了附件、附件是否进了 candidate**」。这比报告 §3 的变量 ① 更强：不只是「region 未命中路径是否被走到」，而是「附件是否根本没产出 / 没进 candidate」。

### 9.5 建议的下一步（交给实现会话）

1. **无条件打印 / 断言**（而非放在 `if (draft.valid)` 内）：09 在插件层 `mapped.skeleton` 的 `skins[0].entries.size()`、`entries[0].attachment.index()`（实际是 Region / Mesh / WeightedMesh / Linked 哪一种）、`triangles.size()`、`vertices.size()`、`regionId.isNone()`。现有诊断块只在 `draft.valid` 为真时执行且只 `CHECK` 不打印，无法区分「附件不存在」与「附件存在但值不同」。
2. **造同口径对照**：把 `domainRejectCode()` 改造成可传入 region 查找（用 06 的真实 regions 代替空 `NameRegions({})`），并让 candidate 也带上图集 —— 与插件层完全同口径后再比一次。
3. 已核实：`docs/skeleton/fixtures/` 里 **09 / 10 / 12 / 13 只有 `.skel.bytes`（无自带 `.atlas.txt` / `.png`）**，插件层借用 06 的图集 ⇒ 附件名与 06 的 region 名大概率不匹配 —— 这正是变量 ① 的具象化，应在对照中显式控制。

### 9.6 停点与纪律

- **不通过 ⇒ 不授权 B5**。定位并闭合 4 个拒绝路径后，由本对话做三次复核。
- 4 个红测试**继续保留为红**（不删 / 不改断言 / 不放宽），作为未闭合证据。
- 数字纪律：smoke 退出码 7 仍是已知共享图集缺口（Task 6），**不得包装成通过**；Full 1113 / Primary 963 双口径不得混用。
- 工作区：B1/B2 返修 + B3 + B4（含返修）叠加栈**全部未提交**（停在 `049b1d3` 后），由负责人手动提交。

---

## 10. 三次复核（返修第三轮，2026-10-23）—— **通过**

### 10.1 根因已定位：一个**真实且严重**的产品缺陷（非测试问题）

`spine32_import_plugin.cpp` 的 Domain 校验闸门此前是**死代码**：

```cpp
const Status status = probe.adoptData(candidate);
if (!status.ok()) { … }        // 旧写法
```

`Status::ok()` 是**静态工厂**（`diagnostics.hpp:129`：`static Status ok() { return Status(); }`），
通过对象调用也只返回一个**新的成功态** ⇒ `!status.ok()` **恒为 false**
⇒ **所有 Domain 校验失败被静默吞掉**。判成功必须用实例方法 `isOk()`（`diagnostics.hpp:138`）。

现改为 `if (!status.isOk())` 并留有警示注释。

**严重性**：插件**从未真正拒绝过任何未通过 Domain 校验的候选工程** —— 第二批的全部拒绝判据（以及既有判据）在此之前都是空转。该缺陷正是被 B4 新增的 4 个拒绝路径测试**暴露**的，再次印证「补断言才能发现真缺陷」（与 B1/B2 重复键同一模式）。

### 10.2 复核证据（本对话独立复跑，非转述）

| 项 | 命令 / 对象 | 结果 |
|---|---|---|
| `cmake` / `mingw32-make`（`build_b4`，含 smoke 目标） | 重跑 | exit **0** / **0**；日志**无 error / warning**；14 个目标全部 `Built`（产物与当前源码一致） |
| ctest | `ctest --output-on-failure` | **7 / 7 通过，exit 0** |
| `loongbones_skeleton_import_tests` | 直接跑二进制 | **40 / 40 通过，exit 0**；`Fixture09 / 10 / 12 / 13` 四项**全部 PASS**，FAIL 计数 **0** |
| `binary_read` 47/47、`spine32_mapper` 48/48、core / format / atlas | ctest #1/#2/#3/#5/#4 | 全绿、零回归 |
| smoke（语料 1113） | 本对话**修复后**独立复跑 | 见 §10.3 |

### 10.3 smoke（修复后复跑 ⇒ 当前证据）

```
单位总数 1113 ｜ prepare 成功 1111 ｜ prepare 失败 2（均为共享图集已知缺口）
完整提交（含重开）8 / 8，重开后：骨架 8 / 图集 8 / 纹理 8 / 区域 243 / 骨骼 331
region 未命中 0 ｜ 尾随空格降级 4
不支持能力账本：animations / atlasPageProperties / deformTimelines / events / timelines
  （第二批 mesh / weightedMesh / linkedMesh / weightedLinkedMesh / ikConstraints / transformConstraints 均不在列）
IMPORT_UNSUPPORTED_CAPABILITY 6636 ｜ 退出码 7
```

⇒ 激活校验闸门**未引入任何新的语料失败**：2 个失败仍是 `h0164s5/watch/` 共享图集已知缺口（Task 6）。
退出码 **7 不得包装成通过**。

### 10.4 各轮 P0 逐项闭合

- **P0-1 ✅**：新增 12 个测试（`test_spine32_plugin.cpp` 18 → 30），端到端 / 账本 / 拒绝三类齐备。
- **P0-2 ✅ 本轮闭合**：4 个拒绝路径（09/10/12/13）**由红转绿**，且断言**未被削弱** —— `CHECK(!draft.valid)` 现为无条件执行，并额外直接调 `validateSkeleton()` 交叉校验域码。
- **P1 ✅**：smoke 在 `build_b4` 重跑；本对话在**修复后**又独立复跑一次（§10.3）。
- **P2 ✅**：`mingw32-make` 退出码 0。
- 二轮复核给出的定位方向（§9.5）已落实：实现会话用**无条件**诊断 + 直接调 `validateSkeleton()` 证明「Domain 在这份骨架上确实报错、但 `adoptData()` 返回 ok」，从而定位到 `ok()` 静态工厂。

### 10.5 新发现（必须单列，**不得**随 B4 通过而被忽略）

**`importers/spine32/src/atlas_import.cpp:318-319` 存在同一缺陷，尚未修复**：

```cpp
const Status status = Project::createWithId(candidate.id, candidate.name).adoptData(candidate);
if (!status.ok()) { … }        // 同一静态工厂陷阱
```

全仓扫描确认：`.ok()` 的非 `isOk()` 用法**仅此一处**（domain / format / tests 其余各处均正确使用 `isOk()`）；`adoptData()` 的**产品侧**调用点也只有两处（插件已修、图集未修）。

⇒ **图集导入路径的 Domain 校验闸门同样是死代码**。

**警告**：修它会使闸门由死变活，`atlas_import_tests`（现 8 项全绿）可能暴露此前被掩盖的真实失败 —— 需连同断言一起改，**不能只改一行了事**。

### 10.6 结论与停点

**B4 三次复核通过。**

- **不自动授权 B5**（阶段 B 纪律）。
- §10.5 的 `atlas_import.cpp` 同类缺陷**必须单独立项修复**，建议作为 B5 之前的一个显式小工作包、由负责人批准；不得因 B4 通过而被略过。
- 数字纪律：smoke 退出码 7 = 已知共享图集缺口，**不得包装成通过**；Full 1113 / Primary 963 双口径不得混用。
- 工作区：B1/B2 返修 + B3 + B4（含三轮返修）叠加栈**全部未提交**（停在 `049b1d3` 后），由负责人手动提交。

---

## 10.7 §10.5 的 atlas 同类缺陷：已修复（复核确认）

实现会话已修 `importers/spine32/src/atlas_import.cpp:318-321`：

```cpp
const Status status = Project::createWithId(candidate.id, candidate.name).adoptData(candidate);
// 【勿写成 status.ok()】… 与 spine32_import_plugin.cpp 同款，两处都已修。
if (!status.isOk()) { … }
```

**复核证据（本对话）**：

| 项 | 结果 |
|---|---|
| 代码 | `isOk()` + 警示注释，与插件侧同款；文件修改于 10-23 05:22 |
| 是否真编译进产物 | ✅ `atlas_import.cpp.obj` 与 `.a` / 测试 exe  rebuilt 于 **05:23:36–38**（晚于源码 05:22） |
| 全仓残留扫描 | ✅ `grep -rn "\.ok()" \| grep -v isOk()` ⇒ **仅剩两处警示注释**（插件 / 图集各一），**无任何代码误用** |
| 重编 | `mingw32-make` exit **0**，日志无 error / warning |
| ctest | **7 / 7 通过，exit 0**；`atlas_import_tests` 仍绿 |
| 闸门由死变活的影响 | ✅ 既有 8 项图集测试**未因此失败** ⇒ 未暴露此前被掩盖的真实失败 |

⇒ **§10.5 的缺陷已闭合**，两处 `adoptData()` 产品调用点的判成功写法现已统一为 `isOk()`。

**残留（须明确记录，不随本项关闭而被忽略）**：`tests/importers/spine32/test_atlas_import.cpp`
**未改动**（仍 8 项，全文零引用 `kDomainRejected` / `adoptData` / `isOk`）⇒ 该拒绝分支**仍无任何断言覆盖**，
即修复前 / 修复后都会 8/8 通过，**无法被测试证伪**。考虑到这一 bug 类型已连续两次「因缺断言而长期隐身」
（B4 插件侧、图集侧各一次），建议补 **≥ 1 条**图集断言：构造未通过 Domain 校验的候选，
断言报出 `kDomainRejected` 且不提交。**此项待负责人批准，不自动开工。**

---

## 10.8 §10.7 残留已闭合：图集拒绝断言已补，并做了**反向探针**验证

负责人指派本对话完成该项（2026-10-23 晚）。

### 10.8.1 新增断言

`tests/importers/spine32/test_atlas_import.cpp` 新增 `RegionExceedingPageIsRejectedByDomain`
（`TEST()` 数 8 → **9**）：

- 输入：页 `size: 32,32`，区域 `xy: 20,0` / `size: 20,20` ⇒ `20 + 20 = 40 > 32`，越出页边界。
- 依据：图集 reader **只校语法与 `size` 形态、不做边界校验**（已核实 `atlas_text_reader.cpp`
  仅报「页 size 必须是两个 int32」）⇒ 该畸形能穿过解析层抵达 Domain；
  Domain 侧 `checkRegion` → `regionPackFitsPage`（`project.cpp:23-31`）失败 ⇒ `kInvalidRegionRect`「矩形越出页边界」。
- 断言：`!d.valid` + `hasCode(kDomainRejected)` + **消息含「矩形越出页边界」**（钉住具体判据，
  避免「任何 Domain 拒绝」都能让测试通过 ⇒ **可被证伪**）+ `commitAtlasImport` 后**不落盘**
  （`commitAtlasImport` 有 `!draft.valid` 前置闸门）。

### 10.8.2 反向探针（关键：证明这条断言真能拦住该缺陷）

此前该分支的问题是「修复前后都会 8/8 通过、无法被证伪」。为证明确已解决，做了对照实验：

| 步骤 | `atlas_import.cpp` 写法 | 结果 |
|---|---|---|
| ① 加断言后 | `status.isOk()`（正确） | atlas **9 / 9 通过**，exit 0 |
| ② **临时**改回缺陷 | `status.ok()`（静态工厂 ⇒ 死代码） | 新测试 **FAIL**，**8 / 9、exit 1** ✅ |
| ③ 还原 | `status.isOk()` | atlas **9 / 9**，ctest **7 / 7**，重编 exit 0 无 error/warning |

⇒ ② 证明该断言**确实是这条死代码的回归护栏**；③ 确认代码已还原、回归全绿。

### 10.8.3 当前状态

- 全仓 `grep "\.ok()"` 仍**无任何代码误用**（只剩两处警示注释 + 新测试里的一句说明性注释）。
- ctest **7/7 exit 0**；`atlas_import_tests` **9/9**；`skeleton_import_tests` 40/40。
- **B4 至此全部闭合**（P0-1 / P0-2 / P1 / P2 + atlas 同类缺陷 + 拒绝断言）。
