# Loongbones 数据类型报告：已实现清单与后续待实现清单

- 日期：2026-10-21
- 范围：`Task4` 阶段 B 收尾时的**类型完整性盘查**，覆盖 `domain/`、`importers/api`、
  `importers/host`、`importers/spine32` 的 canonical 类型
- 判定：被误删后重建的实现文件，其**声明的符号与字段全部有定义，零缺失**；
  但 canonical 体系按设计**尚未覆盖** mesh 族 / 动画 / 约束 / 事件

---

## 1. 本次盘查做了什么

因为静态库存在一类**静默失效**：头文件中声明、`.cpp` 中未定义的函数，
只要没人调用，链接就不会报错。本次因此不依赖"能编译通过"当作完整性证据，
而是逐项核对 **头文件声明 vs `.cpp` 定义**、以及**结构体字段 vs 赋值点**。

同时确认：13 个 `tools/` 目录全部在根 `CMakeLists.txt` 注册；扫描全部 `.cpp`，
**0 个孤儿源文件**（存在但不参与构建）。

---

## 2. 已确认实现的类型

### 2.1 重建文件的核对结果（重点）

`importers/spine32/src/atlas_import.cpp` 是本次唯一被误删后**重建的实现文件**
（另有两个测试文件属重写，非实现）。核对结果：

| 声明（`atlas_import.hpp`） | 定义（`atlas_import.cpp`） | |
|---|---|---|
| `inspectPng` | :153 | ✓ |
| `prepareAtlasImport` | :200 | ✓ |
| `commitAtlasImport` | :336 | ✓ |
| `toString` | :551 | ✓ |
| `FileSystemResourceResolver` ctor / `read` / `valid` | :388 / :396 / 内联 | ✓ |
| `DirectoryResourceStaging` ctor / dtor / `stage` / `publish` / `rollback` / `complete` / `valid` | :431 / :439 / :443 / :504 / :531 / :544 / 内联 | ✓ |

**4 个自由函数 + 2 个类 10 个成员函数，全部有定义。**

字段赋值核对（`AtlasImportDraft` 六个字段，即重建漏东西的高发区）：

| 字段 | 赋值点 |
|---|---|
| `textures` | :308 |
| `pageCount` | :311 |
| `regionCount` | :312 |
| `diagnostics` | :320（经 `addDiag`） |
| `document` | :331 |
| `valid` | :332 |

旁证：全量编译**零警告**；`atlas_import_tests` **8/8**；语料 1113 个单位 prepare 全通过。

### 2.2 canonical 类型总表（`domain/`）

| 头 | 类型 |
|---|---|
| `ids.hpp` | `Id`（ULID，26 字符 Crockford Base32）、`IdKind`、Project/Scene/Node/ImageAsset 别名 |
| `transform.hpp` | `Transform` |
| `image_asset.hpp` | `ImageAsset`、`ImageAssetType`、`StorageMode` |
| `atlas.hpp` | `TextureAsset`、`AtlasPage`、`AtlasRegion`、`Atlas`、`RegionRotation` + 4 个 Id 别名 |
| `node.hpp` | `NodeCommon`、`NodeRecord`、`GroupNodeData`、`ImageNodeData`、`SpriteNodeData`、`NodeType` |
| `skeleton.hpp` | `Skeleton`、`Bone`、`Slot`、`Skin`、`SkinEntry`、`RegionAttachment`、`SourceRef`、`ColorF`、`BlendMode` + 5 个 Id 别名 |
| `project.hpp` | `Project`、`ProjectData`、`Scene`、`ChildPolicy` |
| `diagnostics.hpp` | `Diagnostic`、`Status`、`DiagnosticCode` |

### 2.3 导入契约类型（`importers/api` + `importers/host`）

| 位置 | 类型 |
|---|---|
| `import_plugin.hpp` | `IImportPlugin`、`ImportRequest` / `ImportDraft` / `ImportReport`、`ImportCode` / `ImportSeverity` / `ImportDiagnostic` / `ImportDescriptor` |
| 宿主服务接口 | `IResourceResolver`、`IResourceStaging`、`IIdFactory` / `RandomIdFactory`、`IProgressSink`、`ICancelToken`、`StagedResource` |
| `import_host.hpp` | `ImportRegistry`、`commitImport()`、`ImportCommitOptions` / `ImportCommitOutcome` |
| `spine32` 实现 | `Spine32ImportPlugin`、`FileSystemResourceResolver`、`DirectoryResourceStaging`、`PngInfo`、`StagedTexture`、`AtlasImportDraft` |

---

## 3. 接下来要实现的类型

这些**不是遗漏**，而是 v2 schema §10「v2 明确不包含的内容」的排除清单。
下表附本次 1113 个语料单位的实测规模，缺口大小可直接量化：

| 待实现 | 语料实测 | 去向 |
|---|---|---|
| **动画 / 全部时间线**（含 deform、事件） | `timelines` **4,988,424**<br>`deformTimelines` 335,817<br>`animations` 16,007<br>`events` 1 | 后续任务。**占比最大的缺口** |
| **WeightedMesh / 权重绑定** | `weightedMesh` **17,271**<br>`weightedLinkedMesh` 2 | 后续任务 + 独立 capability |
| **MeshAttachment / LinkedMesh** | `mesh` **2,979** | 同上 |
| **BoundingBox 附件** | 类型 1 | 同上 |
| **IK / Transform / Path 约束** | `ikConstraints` **2,707**<br>`transformConstraints` 208 | 同上 |
| `SkeletonNode`（场景中的骨架实例） | — | 会改 v1 `SceneNode::type` 枚举，**另立任务**（D27） |
| 多皮肤换装 / 皮肤混合 | — | 后续任务（语料无真实样本，T-B4） |
| 外部引用纹理的导入产出 | — | 保留 `external` 枚举值供用户显式使用，**导入器不产出** |

### 配套待验证项（`skeleton-domain-rfc.md` §11.1）

| 项 | 内容 | 状态 |
|---|---|---|
| T-A1 | `rotation≠0` 与页缩放复合 | 待验证 |
| T-A2 | 继承布尔分布 | 待验证 |
| T-B1 | `pma:true` | 待验证 |
| T-B2 | `split` / `pad` | 待验证 |
| T-B3 | 尾随空格降级匹配 | **已由语料实测 0 例降级 + B6 测试覆盖，可结** |
| T-B4 | 多皮肤 / 多页图集 | 待验证（语料全是单页） |
| T-B5 | `deform offset` 形态分布 | 待验证 |

---

## 4. 残余风险（诚实记录）

1. **重建版无法自证与原文件语义等价**。原文件已不存在、仓库无 git；
   中文诊断文案是重新撰写的，非逐字原文（原文件更早时被一次 ANSI/UTF-8
   整文件往返二次编码损坏，且不可逆）。本轮就从重建版里抓到过
   **一个真实缺陷**（纹理未写进 `candidate.textures`，提交后 `project.json`
   声明纹理资产但磁盘上没有 PNG），靠测试 SIGSEGV 才发现 —— 说明
   **同类残留不敢保证没有**。已在文件头补记完整经过。

2. **共享图集布局不受支持**：语料 `h0164s5/watch/` 下 2 个单位共用与
   **目录**同名的图集（如 `h0164s5_watch.atlas.txt`），与插件的
   「同名替换」规则不符 → `IMPORT_RESOURCE_MISSING`。支持它需要给
   `IResourceResolver` 增加**目录列举**能力，属后续工作。

3. **证据范围**：本目录结论全部来自 Spine 3.2.01 语料，
   **尚未完成 DragonBones 对照**，不得表述为跨格式通用结论。

---

## 5. 本次同步的状态变更

- 修正 8 处文档状态不一致（`STRUCTURE.md` ×4、`Task4.md`、`Task3.md`、
  `B1_REPORT.md`、`Task4_Implementation_Plan.md`）
- 修正 2 处悬空路径引用（`tools/format-tests/src/test_v2_skeleton.cpp`
  → `tests/format/test_v2_skeleton.cpp`）
- `tools/format-tests/` 补登记进 `STRUCTURE.md` §2 工具登记簿（§2.13，共 13 项）
- 构建产物统一移出仓库：见下节

### 5.1 构建目录位置（已变更）

历史遗留的 14 个 `build*/` 目录已全部移出仓库，集中到与 `.devtools` 同级的位置：

```text
D:\Works\Spine\.builds\        ← 仓库外，不入 git
    main/                      ← 当前使用的构建根
    build/  build_b1/  build_b2/  build_b3/  build_b6/  build_b6d/
    build_bt3/  build_bt3_final/  build_bt3_pre/
    build_t2/ … build_t6/      ← 历史缓存，已失效，仅归档
```

> **注意**：CMake 的 `CMakeCache.txt` 记录**绝对路径**，因此被移动的 14 个历史
> build 目录**均已失效**，无法直接增量重建；它们在 `.builds/` 下仅作归档。
> 实际使用请用下面的 `main`。

后续构建请指向仓库外：

```powershell
cd d:\Works\Spine\Loongbones
cmake -S . -B ..\.builds\main -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build ..\.builds\main -j 8
ctest --test-dir ..\.builds\main
```

语料扫描工具（默认 OFF）按需开启，例如
`-DLOONGBONES_BUILD_SKELETON_IMPORT_SMOKE=ON`。
