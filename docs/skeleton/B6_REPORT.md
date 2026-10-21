# Task 4 B6：静态插件宿主、事务提交与端到端闭环

**状态：实现完成，待负责人复核。** 25 项自动化测试通过（宿主事务 10 + 插件集成 15）；
真实语料 **1113 个单位** 全部跑过 prepare：**成功 1111、失败 2**（共享图集布局，见 §8），
**region 未命中 0**，**8 个单位** 完成「prepare → 提交 → 重开」闭环且对象计数逐项一致；
全部 7 个 CTest 目标绿。

> 注意口径：1113 是**跑过**的单位总数，不是全部通过 —— 有 2 个 prepare 失败。
> 引用数字时请区分「总数 1113」与「成功 1111」。

---

## 1. 交付物

| 文件 | 作用 |
|---|---|
| `importers/api/include/…/import_plugin.hpp` | 契约：`IImportPlugin`、`ImportRequest` / `ImportDraft` / `ImportReport`、诊断码、宿主服务接口 |
| `importers/api/src/import_plugin.cpp` | `ImportRequest::cancelled()` / `progress()`、`toString(ImportCode)` |
| `importers/api/src/sha256.cpp` | `sha256Hex()` |
| `importers/api/src/path_util.cpp` | `isSafeRelativePath()` / `normalizedRelative()` |
| `importers/host/include/…/import_host.hpp` | `ImportRegistry`、`commitImport()`、`ImportCommitOptions` / `Outcome` |
| `importers/host/src/import_host.cpp` | 注册表与提交事务实现 |
| `importers/spine32/include/…/spine32_import_plugin.hpp` | Spine 3.2.01 插件声明 + descriptor 常量 |
| `importers/spine32/src/spine32_import_plugin.cpp` | `prepare()`：二进制 → 图集 → 映射 → 域校验 |
| `importers/spine32/src/builtin_registration.cpp` | `registerBuiltinPlugins()` 的**插件侧**定义 |
| `tests/importers/test_import_host.cpp` | 10 项宿主事务断言 |
| `tests/importers/test_spine32_plugin.cpp` | 15 项插件集成断言 |
| `tools/skeleton-import-tests/` | 上述 25 项的构建入口（进 CTest） |
| `tools/skeleton-import-smoke/` | 无 GUI 最小宿主 + 语料端到端验证（默认 OFF） |

改动：`importers/api/CMakeLists.txt`（新）、`importers/host/CMakeLists.txt`（新）、
`importers/spine32/CMakeLists.txt`、`importers/spine32/include/…/atlas_import.hpp`、
根 `CMakeLists.txt`。

---

## 2. 分层与依赖方向

```text
importers/api   ←  契约（无依赖，除 domain / project_document）
    ↑                    ↑
importers/host        importers/spine32
（只依赖 api）        （依赖 api + host，并提供注册入口）
```

- **宿主不依赖任何具体插件**：`import_host.cpp` 里没有 `#include` 任何 `spine32` 头。
- **注册入口由插件侧提供**：`registerBuiltinPlugins()` 的**定义**在
  `importers/spine32/src/builtin_registration.cpp`，**声明**在 `import_host.hpp`。
  于是依赖方向始终是「插件 → 宿主」，不会反向。
- **文件系统的具体实现留在插件侧**：`FileSystemResourceResolver` /
  `DirectoryResourceStaging` 仍在 `importers/spine32/src/atlas_import.cpp`。
  宿主只依赖 `IResourceResolver` / `IResourceStaging` 接口。

### 2.1 为什么不用「静态初始化器自动注册」

常见写法是让每个插件放一个全局对象，构造函数里往单例注册表塞自己。
**在静态库里这会静默失效**：链接器只拉入被引用的目标文件，
一个只有静态初始化器、没有任何外部符号被引用的 `.obj` **不会被链接进来**，
于是插件"注册了但没注册"，而且没有任何编译或链接错误。

B6 因此采用**显式调用**：可执行文件在启动时（或 smoke 的 `main()` 里）
明确调用 `registerBuiltinPlugins(registry)`。注册是否生效可被断言覆盖
（`RegisterBuiltinPluginsMakesItFindable`）—— 自动注册做不到这点。

---

## 3. `prepare()` 契约

一次导入 = 一个「单位」= `.skel.bytes` + 同名 `.atlas.txt` + 图集页引用的 PNG。

| 阶段 | 内容 | 失败诊断 |
|---|---|---|
| 0 | 源路径必须落在授权根内（词法检查） | `IMPORT_SOURCE_REJECTED` |
| 0 | 取消令牌 | `IMPORT_CANCELLED` |
| 1 | 读 `.skel.bytes` | `IMPORT_SOURCE_MISSING` |
| 2 | `readSkeletonBinary32` 解析 | `IMPORT_PARSE_ERROR` |
| 3 | 读同名 `.atlas.txt` + `readAtlasText` | `IMPORT_RESOURCE_MISSING` / `IMPORT_PARSE_ERROR` |
| 4 | 逐页读 PNG + `inspectPng`（只取 IHDR 尺寸） | `IMPORT_RESOURCE_MISSING` / `IMPORT_RESOURCE_INVALID` |
| 5 | `mapSkeletonToCanonical` + region 查找 | `IMPORT_UNRESOLVED_REFERENCE` / `IMPORT_ATTACHMENT_REGION_DEGRADED` |
| 6 | 完整候选副本 → 临时 `Project::adoptData()` 校验 | `IMPORT_DOMAIN_REJECTED` |

三条硬约束：

1. **不改 `request.current`** —— 候选是 `ProjectData` 的完整副本，追加对象后
   用**临时** `Project` 实例做全量校验，不增量调用 Project 写 API。
2. **不写盘** —— PNG 只进 `draft.resources`（暂存清单），落盘是 `commitImport()` 的事。
3. **error 全拒绝，不留半完成** —— `mapped.valid` 为假时直接返回，
   `draft.valid` 保持假，宿主不会 stage 任何东西。

### 3.1 纹理资产名字去重

`prepare()` 先把**现有** `current.data.textures` 的名字收进 `usedTextureNames`，
再为每个新建纹理取不重复的名字。多次导入同一工程时页文件名可能撞车，
不去重会让工程里出现同名纹理资产。

---

## 4. 提交事务

`commitImport(draft, options, staging, inOutDocument)` 是**唯一**会改磁盘的入口。

```text
闸门（invalid / 有 error / warning 未确认）  → 不写任何东西
  ↓
1) stage()    写 <target>.stage-<random> 并**回读比对**
  ↓
2) publish()  原子落到最终路径；**拒绝覆盖**已存在文件
  ↓
3) saveProject()  原子替换 project.json（资源已就位，无悬挂窗口）
  ↓
4) complete() 确认保留已发布文件，清理临时文件
```

任一环节失败 → `rollback()`，只删**本事务创建**的文件
（`DirectoryResourceStaging` 记录每条 `published` 状态），旧工程保持有效。
析构函数兜底：未 `complete()` 的事务在离开作用域时自动回滚。

`publish()` 的实现：Windows 用 `MoveFileExW(..., MOVEFILE_WRITE_THROUGH)`，
POSIX 用 `link` + `remove` —— 都是原子安装且不覆盖。

**崩溃语义**：进程在任何时刻崩溃，最多遗留未被引用的资源文件；
不会出现 `project.json` 指向缺失文件的窗口 —— 因为 `project.json`
的替换发生在资源全部发布**之后**。

---

## 5. 测试（25 项）

`ctest -R skeleton_import_tests`。输入全部为**内存合成**。

### 5.1 宿主事务（`test_import_host.cpp`，10 项）

| 断言 | 期望 |
|---|---|
| 注册表精确匹配 | `spine.binary@3.2.01` 命中；版本不符 / 格式不符返回 `nullptr` |
| 注册 `nullptr` | 被忽略，不计入 descriptors |
| 无效草案 | `committed == false`，不 stage、不落盘 |
| warning 未确认 | `IMPORT_WARNINGS_NOT_ACCEPTED`，不写任何文件；确认后可提交 |
| 暂存失败 | `IMPORT_RESOURCE_STAGE_FAILED`，rollback 1 次，无残留文件 |
| 发布失败 | `IMPORT_RESOURCE_STAGE_FAILED`，`complete()` 未被调用 |
| 真实提交 | 资源落盘、无 `.stage-*` 残留、宿主文档被更新 |
| 保存失败 | `IMPORT_PROJECT_SAVE_FAILED`，资源回滚 |
| 授权根 | `..` / 绝对路径 / 空路径一律拒绝；根内相对路径可读 |
| 回滚保留既有文件 | 同名既有文件不被覆盖、内容未被改写 |

### 5.2 插件集成（`test_spine32_plugin.cpp`，15 项）

| 断言 | 期望 |
|---|---|
| descriptor | `loongbones.spine32` / `spine.binary` / `3.2.01` |
| `canImport` | 格式与版本都精确匹配才为真 |
| 内置注册 | `registerBuiltinPlugins()` 后可被注册表找到 |
| 最小单位 | 1 骨 / 1 槽 / 1 皮肤 / 1 region 附件；草案有效 |
| 资源位置 | `draft.resources[i].projectLocation == textures[i].location` |
| 版本标记 | `sourceVersion == kV2` |
| prepare 不改 current | 前后对象计数逐项一致 |
| 查找键 | 无 `path` 时用附件名命中 |
| 尾随空格 | 降级命中 → `IMPORT_ATTACHMENT_REGION_DEGRADED`（warning，恰好 1 条） |
| 未命中 | `IMPORT_UNRESOLVED_REFERENCE`（error），草案无效 |
| 缺 `.atlas.txt` / 缺 PNG | `IMPORT_RESOURCE_MISSING` |
| 非法 PNG | `IMPORT_RESOURCE_INVALID` |
| 取消 | `IMPORT_CANCELLED`，草案无效 |
| 越权源路径 | `IMPORT_SOURCE_REJECTED` |
| `contentHash` | 等于对 `.skel.bytes` 全文的 SHA-256 |
| 端到端 | 提交后资源落盘、`project.json` 存在；**重开**对象数与提交前一致 |

---

## 6. 语料端到端实测（2026-10-21）

`tools/skeleton-import-smoke/`，语料根
`…/assets/game/character/spinedata`，**只读**。

```
单位总数                    1113
prepare 成功                1111
prepare 失败                 2
完整提交（含重开）            8（上限 8）
落盘资源文件                  8
region 附件                   243
尾随空格降级                  0
region 未命中                 0
进度回调                      5561 次（异常 0）

重开后：骨架 8 / 图集 8 / 纹理 8 / 区域 243 / 骨骼 331
```

不支持能力（只计数 + warning，不建半成品对象）：
`timelines 4,988,424` / `deformTimelines 335,817` / `weightedMesh 17,271` /
`animations 16,007` / `ikConstraints 2,707` / `mesh 2,979` /
`transformConstraints 208` / `weightedLinkedMesh 2` / `events 1`。

> 说明：完整提交的 8 个单位累加出 243 个 region，不是全语料的 27,457 ——
> smoke 只提交前 8 个单位，其余 1105 个只做 prepare 计数。

---

## 7. 本轮修掉的三个真实缺陷

这三个都不是"测试写错了"，而是**实现本身有洞，靠测试才暴露出来**：

1. **`prepare()` 从不填充 `draft.resources`**
   —— 纹理只进了 `textures` 向量，暂存清单始终为空，
   于是"提交"出来的工程里 `project.json` 声明了纹理资产，
   磁盘上却**一个 PNG 都没有**。典型悬挂引用。

2. **B1 `prepareAtlasImport()` 不把纹理写进 `candidate.textures`**
   —— 页引用了纹理资产 ID，但候选工程数据里没有这条记录，
   `document.data.textures` 为空。上一轮重建 `atlas_import.cpp` 时漏掉，
   测试访问 `textures[0]` 直接 SIGSEGV 才被发现。

3. **`FileSystemResourceResolver::read()` 把"文件不存在"报成"越权"**
   —— `fs::canonical()` 对「路径不存在」和「无法解析」返回同一种失败，
   原实现一视同仁地报「解析后的路径越出授权根目录」。
   后果是排查方向被带偏：明明是语料缺文件，却去查符号链接逃逸。
   现已拆分为「资源不存在」/「无法解析资源路径」/「越出授权根目录」三种。

---

## 8. 边界与已知缺口

**做到的**：

- 静态插件契约 + 注册表 + 事务提交 + 重开闭环；
- 资源先发布、`project.json` 后替换，无悬挂窗口；
- 失败只回滚本事务文件，既有文件不动；
- 授权根越界 / 绝对路径 / 空路径一律拒绝；
- 语料 1113 个单位全部跑过 prepare：成功 1111、失败 2（共享图集布局），region 未命中 0。

**没做的**：

- **Mesh 族 / 动画时间线 / IK / transform / 事件仍不进 canonical** ——
  与 B4/B5 同一条边界，只计数 + warning。本轮语料实测
  `timelines` 高达 4,988,424 条，全部落在这条边界之外。
- **共享图集布局不支持（已知缺口，2 个单位 → prepare 失败）**：
  这是 B6 里**唯一**导致 prepare 失败的原因。语料 `h0164s5/watch/` 下两个骨架
  （`h0164s5_watch_hour` / `h0164s5_watch_minute`）共用
  `h0164s5_watch.atlas.txt`，与插件的「同名替换」规则不符 →
  `IMPORT_RESOURCE_MISSING`。支持它需要给 `IResourceResolver`
  增加**目录列举**能力，超出 B6 范围，如实标注。
- UTF-8 校验仍未做（`kInvalidUtf8` 预留）。
- 无 GUI：交互入口仍只有 `tools/skeleton-import-smoke`。
