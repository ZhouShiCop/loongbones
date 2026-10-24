# Task 6 阶段 B 交付报告（共享图集布局的导入支持）

- 上游：`Agent_Task/Task6_Implementation_Plan.md` §7；阶段 A 的 A1/A2/A3 已交付（`Agent_Task/Task6.md`）
- 授权：负责人 2026-10-24「B阶段可动」
- 日期：2026-10-24
- 状态：**B1 / B2 完成，语料闭环验收口径达成（smoke 退出码 0）**；**B3 的单元测试与 A4 合成 fixture 未做**

---

## 1. B1：宿主接口与实现

| 文件 | 改动 |
|---|---|
| `importers/api/include/loongbones/importers/api/import_plugin.hpp` | `IResourceResolver` 新增 `listDirectory()` —— **带默认实现（返回 false = 不支持列举）**，既有实现类（宿主 + 3 个测试 `FakeResolver`）**无需改动**，向后兼容；默认实现保证"列举不可用 ⇒ 退化为原骨架同名解析"，**不会**放宽授权根校验 |
| `importers/spine32/include/loongbones/importers/spine32/atlas_import.hpp` / `src/atlas_import.cpp` | `FileSystemResourceResolver` 覆盖 `listDirectory()`：授权根校验（`isSafeRelative` / `canonical` / `pathWithin`）与 `read()` **完全一致**；失败分类沿用 B6 三种（目录不存在 / 无法解析 / 越出授权根）；只列**一层普通文件**的文件名、不递归、按名称**确定性排序** |

依赖方向未变：接口在 `importers/api`，**文件系统实现仍在插件侧**（`importers/spine32/`）。

## 2. B2：插件匹配逻辑

`importers/spine32/src/spine32_import_plugin.cpp` 第 3 步（读 `.atlas.txt`）按 RFC §4.3 的优先级实现：

1. **骨架同名** `<stem>.atlas.txt` —— 既有 1111 个单位的路径**完全不变**（提前返回，不进入列举）；
2. 失败 ⇒ `listDirectory()` 取目录内 `.atlas.txt`：
   - **恰好 1 个** ⇒ 采用（共享图集布局）；
   - **≥2 个** ⇒ **歧义：显式拒绝 + 可定位诊断**（列出候选数），**不猜测、不选第一个**；
   - **0 个 / 列举不可用** ⇒ 维持原 `IMPORT_RESOURCE_MISSING` 诊断（既有行为不变）。

新增诊断码 `kResourceAmbiguous`（`IMPORT_RESOURCE_AMBIGUOUS`），**只追加**到 `ImportCode` 末尾，不改既有码语义。

`prepare()` 的事务语义、暂存清单 `draft.resources` 的填充、纹理名去重（B6 §3.1）**均未改动**。

## 3. 验证

构建目录 `D:\Works\Spine\.builds\build_b5`（Task 5 B5 建的干净目录，本次为**增量重编**：cmake 0 / make **0**，**error 0 / warning 0**）。

| 项 | 结果 |
|---|---|
| ctest | exit **0**，**7/7 通过，0 失败 0 跳过** |
| `binary-read-smoke`（双口径） | exit **0**；Full **1113 ok 1113 fail 0** / Primary **963 ok 963 fail 0** |
| `skeleton-import-smoke`（端到端） | exit **0** ✅ |

`skeleton-import-smoke` 关键数字：

```
单位 1113
prepare 成功 1113     ← 原 1111
prepare 失败   0      ← 原 2（h0164s5_watch_hour / _minute）
完整提交（含重开） 8/8
region 未命中 0
[deep-compare] items=18067 maxResidual=0 mismatches=0
```

⇒ **B3 验收口径达成**：`1113 / 1113 prepare 成功、失败 0`、region 未命中仍 0、完整提交 + 重开一致、**退出码 0**（可作验收证据）。
诊断汇总里已无 `IMPORT_RESOURCE_MISSING` / `IMPORT_RESOURCE_AMBIGUOUS`。

## 3.1 最终验收（**全新干净目录** `build_t6`，B3 完成后复跑）

计划 §7 / R2 要求「干净构建目录、不得复用历史缓存」。本轮新建
`D:\Works\Spine\.builds\build_t6`（全新），全量重编后跑全部验收。

| 项 | 结果 |
|---|---|
| cmake 配置 | exit **0** |
| 编译（`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`） | exit **0**，**error 0 / warning 0** |
| ctest | exit **0**，**7 / 7 通过，0 失败 0 跳过** |
| `binary-read-smoke` | exit **0**；Full **1113 ok 1113 fail 0** / Primary **963 ok 963 fail 0** |
| `skeleton-import-smoke` | exit **0** ✅ |

各套件断言（干净目录下的新二进制）：

| 套件 | 基线 | 本轮 | 结果 |
|---|---:|---:|---|
| `loongbones_core_tests` | 135 | 135 | ✅ 零回归 |
| `loongbones_format_tests` | 86 | 86 | ✅ 零回归 |
| `loongbones_binary_read_tests` | 47 | 47 | ✅ 零回归 |
| `loongbones_spine32_mapper_tests` | 48 | 48 | ✅ 零回归 |
| `loongbones_skeleton_import_tests` | 40 | **46** | ✅ **+6**（B3 新增单测） |
| `loongbones_atlas_import_tests` | 9 | 9 | ✅ 零回归 |

端到端关键数字：

```
单位 1113 / prepare 成功 1113 / 失败 0
完整提交（含重开） 8/8   region 未命中 0
[deep-compare] items=18067 maxResidual=0 mismatches=0
```

诊断汇总里**无** `IMPORT_RESOURCE_MISSING` / `IMPORT_RESOURCE_AMBIGUOUS`。

### 环境事实（本次踩到，务必记）

**`CXX` 环境变量优先于 `-DCMAKE_CXX_COMPILER`**：首次配置时 CMake 用环境变量 `CXX`
初始化编译器缓存 ⇒ 我传的 `-DCMAKE_CXX_COMPILER=…/g++.exe` 被**环境变量里的
已删除 `c++.exe` 覆盖** ⇒ `CMake Error: … is not a full path to an existing compiler tool`。
处置：构建脚本里**显式设 `$env:CXX`** 指向存在的 `g++.exe`（不只传 `-D`）。
（另：本次干净目录下 `mingw32-make` 退出码 **0**，此前 `build_b5` 增量构建出现过的 2 未再复现。）

## 4. 过程中的事故（自己造成，已修复，如实记录）

编辑 `atlas_import.cpp` 时，一次替换**误删了 `toString(AtlasImportCode)` 的函数体**（switch 剩余 case、闭合括号、namespace 闭合）。
在下一次编译前发现并完整恢复（现文件末尾 = 完整 `toString` + 新增 `listDirectory` + namespace 闭合），
随后 `make exit 0`、ctest 全绿 ⇒ 未流入验证结果。教训：对"文件末尾/大段函数"做替换时要先确认片段边界。

## 5. 未完

| 项 | 说明 |
|---|---|
| **B3 单元测试** | ✅ **已补 6 个**（`tests/importers/test_spine32_plugin.cpp`）：`SharedAtlasResolvesBothSkeletons`（17 正向共享：两骨架都成功、各 1 图集 1 纹理、区域数一致）、`AmbiguousAtlasCandidatesAreRejected`（18 歧义 ⇒ `kResourceAmbiguous`，且**不得**落成资源缺失）、`SameNameAtlasWinsOverOtherCandidate`（19 同名优先：不得判歧义、不得取 decoy）、`DirectoryListingFailureFallsBackToResourceMissing`（列举失败 ⇒ 退化资源缺失，**不放宽**校验）、`ListingOutsideAuthorizedRootIsRejected`（真实 `FileSystemResourceResolver`：`..` / `sub/../../..` 必须被拒；合法列举只返回一层普通文件名）、`ListingMissingDirectoryIsRejected`（目录不存在 ⇒ 失败且有 error）。套件断言 **40 → 46** |
| ~~A4 合成 fixture~~ | ✅ **已由阶段 A 补齐**（17 正向共享 / 18 歧义 / 19 同名优先三个目录型 fixture + golden），本轮直接用作 B3 单测的可复现输入 |
| 共享资产语义（A3 推荐"共用 Atlas/TextureAsset"） | **未实现、未验证**：两个骨架是否指向同一个 `Atlas.id`、工程内是否各只有 1 份，需 B3 单测确认 |
| 提交 | 全部改动**未提交**（含 Task 5 的 B1–B5 叠加栈） |

## 6. 停点

B1 / B2 完成且语料闭环 exit 0；**B3 单测与 A4 fixture 未做**。
按「一个工作包的批准不自动授权下一个」与「不得在断言缺失时宣称完成」的纪律，此处停下汇报，
是否继续补 B3 单测 + A4 fixture 请你定。
