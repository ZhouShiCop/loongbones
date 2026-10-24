# Task 5 阶段 B：B4 返修报告（第二轮，含二次复核后的事实修正）

- 上游：`Agent_Task/Task5_B4_Review.md`（**二次复核仍不通过**：P0-1 / P1 / P2 已闭合，P0-2 的 4 个拒绝路径未闭合，ctest 6/7）
- 日期：2026-10-23
- 状态：**P0-1 达成；P1 / P2 闭合；P0-2 的 4 个拒绝路径仍未闭合**（测试保留为红）

## 1. 返修清单对照（复核二次确认）

| 复核条目 | 状态 | 复核核实 | 本轮补充 |
|---|---|---|---|
| **P0-1** 新增 ≥ 10 个测试 | ✅ | `TEST()` 数 18 → 30（新增 12；报告原写 17→29 属计数口径偏差，不影响结论） | 已按复核口径修正 |
| **P0-2** 真实三件套插件端到端 | ⚠️ 部分 | 8 个正向 / 账本 ✅；**4 个拒绝路径 ❌ 保留为红** | 见 §2：范围已收窄，但**仍未闭合** |
| **P1** smoke 在 build_b4 重跑 | ✅ | 完整提交 8/8、region 未命中 0；退出码 **7** = 已知共享图集缺口，未包装成通过 | 同 |
| **P2** make 退出码 2 根因 | ✅ | 退出码回到 **0**；根因 = 首次配置未启用 smoke 目标 + 缓存不完整 | 同 |
| **P3** B5 | 未开工 | 按停点纪律，B4 不通过即不授权 | 同 |

## 2. 4 个拒绝路径：范围收窄（并**撤回**上一轮报告里的一个错误结论）

### 2.1 撤回：mapper 层的「被拒」**不构成**插件层对照（复核 §9.3）

上一轮报告 §3 写「差异不在 Reader、不在 Domain 校验、不在 adoptData 本身」—— **该结论未被证明**，
因为两边的输入**不同口径**：

| | region 查找 | candidate 组成 |
|---|---|---|
| mapper 层（`domainRejectCode()`） | 人工 `NameRegions({})`（**空** ⇒ 全部未解析） | **只有** skeleton |
| 插件层 `prepare()` | **真图集** `AtlasRegionIndex({atlas})` | current.data + **textures** + **atlas** + skeleton |

⇒ 那句结论撤回，改为「差异未定位」。

### 2.2 本轮新查到的事实（缩小范围用）

1. **插件 §5 第 316 行：`mapperOptions.atlasIds = {atlas.id};`** —— 插件给骨架**挂了图集 ID**，
   mapper 层对照里**没有**这一项。这会改变 Domain 侧 `checkAtlasRefs` 的判定输入。
2. **Domain 侧本身没问题**：`tests/domain/test_skeleton.cpp` 的
   `MeshTriangleIndexOutOfRangeRejected` 用的是 `validProjectWithSkeleton()`（**含真实图集**）
   + `Project::adoptData()` ⇒ **在带图集的工程里，坏 mesh 确实被拒** ✓。
   ⇒ 嫌疑范围收窄到**插件装配 / 映射路径**，不是 Domain 校验本身。
3. **插件层 draft 里附件确实在**：上轮加的诊断断言（`std::get_if<MeshAttachment>` 非空、
   `triangles.size()==3`、`triangles[2]==9`）**全部通过** ⇒ 附件以 `MeshAttachment` 形态进了
   `draft.document`（= candidate）。而同一时刻 `adoptData()` 却**放行**。
   **这两件事并存，当前代码路径解释不了**。
4. **决定性实验（本轮新增，无条件执行）**：在插件测试里对 `draft.document.data.skeletons[0]`
   **直接**调 `validateSkeleton(s, draft.document.data)`：
   `CHECK(!domainDiags.empty())` ✅、`CHECK_EQ(domainDiags.front().code, kMeshTriangleIndexOutOfRange)` ✅
   ⇒ **Domain 校验在这份骨架上确实报错**，但 `probe.adoptData(candidate)` 仍返回 ok
   ⇒ 两者对「同一份数据」结论不一致。
5. **已排除**：`ProjectData` 是**纯聚合**（`domain/include/loongbones/domain/project.hpp:41-60`，
   无自定义拷贝 / 移动构造）⇒ 不存在「按值传参时 skeletons 丢失」。
6. **剩余待查（下一步入口，精确到行）**：`Project::adoptData()`（`domain/src/project.cpp:1018`）
   内建 `Project candidate; candidate.data_ = std::move(data);` 再 `candidate.validate()`；
   `Project::validate()` 的骨架循环在 `project.cpp:984-996`。
   需确认**该循环在 adoptData 场景下是否真的执行**（validate() 是否有早期 `return out`），
   这是目前唯一能解释「直接 validateSkeleton 报错、adoptData 却放行」的位置。

### 2.3 下一步（唯一手段）

按复核 §9.5 第 1 条，把诊断改成**无条件**执行并**打印**事实：附件种类索引、`entries.size()`、
`triangles.size()`、`vertices.size()`、`regionId.isNone()`，以及 `mapped.skeleton` 与
`candidate.skeletons[0]` 是否同一内容；再按第 2 条做**同口径对照**（region 查找用真图集、
candidate 带图集与 textures）。

**本轮未完成该定位**（token 预算已尽），4 个红测试**继续保留为红**（不删 / 不改断言 / 不放宽）。

## 3. 验证

| 项 | 结果 |
|---|---|
| 构建 | `build_b4`（干净配置，含 `-DLOONGBONES_BUILD_SKELETON_IMPORT_SMOKE=ON`）：cmake **0** / make **0**，日志零 error / warning |
| ctest | **6 / 7** —— `skeleton_import_tests` 失败（正是 09/10/12/13 四项）；其余 6 个套件全绿 |
| smoke | 完整提交 **8 / 8**、region 未命中 0；退出码 **7** = 已知共享图集缺口（Task 6），**不包装成通过** |
| 既有断言 | 本轮**未改任何既有断言**（只新增） |

## 4. 环境事实

- `.devtools\mingw64\bin\c++.exe` **已不存在**（只剩 `g++.exe` / `clang++.exe`）⇒ 旧构建目录 cache 全部失效，
  须新建干净目录并显式 `-DCMAKE_CXX_COMPILER=…/g++.exe`、`-DCMAKE_MAKE_PROGRAM=…/mingw32-make.exe`。
- **PowerShell 5.1 把 BOM-less UTF-8 脚本按 ANSI 读** ⇒ 脚本里的中文注释会**吞掉下一行**（实测 `$bin` 变空串）。
  后台脚本一律**纯 ASCII**。

## 4.5 附带修掉的第二处 `.ok()` 陷阱（负责人指出）

- **位置**：`importers/spine32/src/atlas_import.cpp:318-319` ——
  `const Status status = ...adoptData(candidate); if (!status.ok()) { ... }`。
- **同款根因**：`Status::ok()` 是静态工厂，通过对象调用恒返回成功 ⇒ **`!status.ok()` 恒为 false**
  ⇒ **图集导入的 Domain 校验闸门同样是死代码**（自 Task 4 B6 起，图集导入从未真正拒绝过
  未通过 Domain 校验的候选）。
- **已修**：改为 `status.isOk()`；两处（本文件与 `spine32_import_plugin.cpp`）都留下
  「勿写成 status.ok()」的注释，避免第三次踩。
- **验证**：修复后 ctest **7/7 全绿**（既有图集导入断言未因闸门生效而失败）；语料闭环 smoke 同步重跑。
- **诚实提示**：与骨架导入那处一样属**修复掩盖型缺陷** —— 此前"不该通过却通过"的图集导入
  现在会被正确拦下。既有测试与 Full 语料未触发，但其它数据可能出现新拒绝。

### 顺带发现（未改，待你裁定）

图集导入与骨架导入的成功路径里都写着
`document.sourceVersion = format::SourceFormatVersion::kV2`（`atlas_import.cpp:329`、
`spine32_import_plugin.cpp:378`），而 B2 起落盘已是 **v3** ⇒ 内存中是 v3 结构却标 v2 来源，
语义不自洽。改动会触碰**既有断言**
`tests/importers/test_spine32_plugin.cpp:350-351`（明确断言 `draft.document.sourceVersion == kV2`），
属契约变更 ⇒ 我**没有擅自改**，等你定。

## 5. 停点

B4 返修：P0-1 达成、P1/P2 闭合，**P0-2 的拒绝路径 4 项未闭合**（测试保留为红，范围已收窄到插件装配路径，
下一步手段已明确）。按计划 §9「不通过不授权 B5」，此处停下等待复核。
