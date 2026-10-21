# Task 2 验收报告（含 Task 2 Back Task）

> 对应规格：`Agent_Task/Task2.md`、`Agent_Task/Task2_Back_task.md`
> 领域模型：`Loongbones/domain/`（Qt 无关，C++17）

## 1. 验收结论

| 项 | 结果 |
|---|---|
| 从干净目录重新配置并编译 | 通过（`cmake -S Loongbones -B Loongbones/build_t5 -G "MinGW Makefiles"`） |
| 测试总数 / 通过 | **67 / 67**，退出码 0 |
| CTest | **1/1** `core_tests Passed` |
| 编译警告（`-Wall -Wextra -Wpedantic`） | **0** |
| 工具链 | CMake 3.31.6 + MinGW-w64 GCC 16.2.0（见 `.devtools/README.md`） |

## 2. Back Task 三大缺口闭合

### 2.1 Scene/root 关系由 `validate()` 完整闭合
- **单场景契约**：`scenes.size() == 1` 且 `defaultSceneIndex == 0`（多场景 → `kSceneCountMismatch`）；`Project::create` 已保证，无场景 → `kSceneNotFound`。
- 场景根必须是存在的 `kRoot` 节点（`kSceneRootInvalid`）；兼容字段 `ProjectData::rootNodeId` 必须与 `scenes[0].rootNodeId` 一致。
- 全工程恰好一个 `kRoot`（`kRootNotUnique`）；该根无父、恒等局部变换、不持有 ImageAsset/AtlasRegion（`kRootParented` / `kRootImmutable` / `kNodeTypeMismatch`）。
- `setDefaultScene()` 仅接受下标 0，并同步 `rootNodeId`。
- `adoptData()` 失败时**只移动候选对象、绝不改动接收方**；首个问题诊断即为 `lastStatus()`。
- 新增回归：`SingleSceneContract` / `MissingSceneRejected` / `MultipleScenesRejected` / `SceneRootMissingNodeRejected` / `SceneRootPointsToOrdinaryNodeRejected` / `DefaultSceneRootMismatchRejected` / `MultipleRootsRejected`。

### 2.2 Atlas 元数据校验覆盖写 API 与导入路径
- 抽离 `checkPage()` / `checkRegion()`（匿名命名空间），**写 API（`addAtlasPage`/`addAtlasRegion`）与 `validate()`/`adoptData()` 共用同一套规则**，杜绝两套规则漂移。
- 覆盖：TextureAsset / Atlas / AtlasPage / AtlasRegion 的 **ID 种类与格式**校验（`kIdKindMismatch`）、**同类型全局 ID 唯一性（跨 Atlas 的 Page/Region 重复拒绝，`kIdDuplicate`）**、Page 引用存在的 TextureAsset、Region 引用**同一 Atlas 内**的有效 Page（`kPageNotInAtlas`）、旋转后实际占位矩形不越页边界、packed 矩形与 original/trimmed 尺寸正值、trim offset 非负且落在 original 内（边界算术用 `int64_t` 防有符号溢出，`kInvalidTrim`）。
- 恶意 `ProjectData` 经 `adoptData()`：越界矩形、trim 非法、ID 种类错误、跨 Atlas 重复 ID、跨 Atlas 引用 Page 均被拒绝，且**接收方快照不变**。
- 新增回归：`GlobalPageIdDuplicateRejected` / `CrossAtlasRegionIdDuplicateRejected` / `RegionCrossAtlasPageRejected` / `OutOfBoundsRectRejected` / `InvalidTrimRejected` / `InvalidRegionIdKindRejected`。

### 2.2b 旋转 Region 的 trim/占位方向（评审退回项，已修正）
- **缺陷**：原 `regionTrimFits()` 在 90°/270° 时按 `rotationSwapsExtent()` 交换 `trimmedW/H` 再与 `originalW/H` 比，而 `atlas.hpp` 将 trimmed/offset 定义为**原图方向**，两者不一致 → **合法的非正方形旋转区域被误判为非法**（64×48 精灵 90° 打包时 `trimmedW(64)` 被拿去比 `originalH(48)`，溢出）。
- **修正**：`regionTrimFits()` **不再转置**（trim/offset 按 original 方向判定）；转置只保留在 `regionPackFitsPage()`（页内实际占位）。并在 `atlas.hpp` 把该方向约定写为权威表述。
- **回归（此前无旋转用例，缺陷因此未被覆盖）**：
  - `RotatedRegionNonSquareAccepted` —— 64×48 非正方形区域以 90°/270° 打包必须**接受**（缺陷版本会拒绝）。
  - `RotatedRegionTransposedOverflowRejected` —— 90° 时页内占位按转置 `(packedH, packedW)` 判定，`packedX=60, packedW=30, packedH=50` 应越界被拒（验证转置确实生效而非被忽略）。
  - `RotatedRegionTrimOverflowRejected` —— trim 按 original 方向判定，`offsetY=5` 应判非法 trim。

### 2.2c 失败导入的完整接收方快照断言（评审退回项，已修正）
- **缺陷**：原导入失败用例用空 `Project` 作接收对象，只断言空容器或单个字段，无法发现"部分写入"。
- **修正**：新增 `makeReceiver()`（已持有场景 + 根 + 分组 + 资源 + 图片节点 + 图集链）与 `sameProjectData()`（对 `ProjectData` 全部字段深比较：id/name/rootNodeId/defaultSceneIndex、scenes、nodes 含 common 全部成员与 children、nodeOrder 顺序、assets、textures、atlases 含 pages 与 regions 全部字段）。所有失败导入用例统一改走 `expectAdoptRejected(malicious, expectedCode)`，断言「拒绝 + 诊断码 + 前后快照完全一致」。

### 2.3 SpriteNode → AtlasRegion 公开创建路径
- 新增 `SpriteNodeData`、`createSpriteNode(parent, data)`、`setNodeRegion(id, regionId)`。
- 只有 `kImage` 节点可引用 Region；`Root`/`Group` 持 Region 由 `typeMatchesData()` 在 `validate()` 拦截（`kNodeTypeMismatch`）。
- 兼容映射（Task 1 ImageAsset/ImageNode ↔ TextureAsset/AtlasRegion）由 `adoptImageAssetAsWholeImageRegion` 提供，已改为**本地构造、整体提交**——任一步失败都**不遗留**新建的 TextureAsset / Atlas / Page。
- 新增回归：`ReferencesRegion` / `NonexistentRegionRejected` / `SetNodeRegionOnNonImageRejected` / `SetNodeRegionClearsAsset`（切换引用时清除 assetId）/ `MultipleSpritesShareRegionIndependentTransforms`（多 Sprite 共享 Region、各自变换独立）。

### 3. 次要契约清理
- `drawOrder()` 语义明确为**含 Root 的确定性全树遍历**（用于导出/迭代）；新增 `drawableOrder()` 返回**不含 Root 的渲染顺序**。新增 `DrawableOrderExcludesRoot` 回归。
- 既有 `EmptyProjectHasOneSceneAndRoot`（`test_model.cpp`）与 `MissingRootDetected`（`test_validation.cpp`，因根缺失语义变为 `kSceneRootInvalid` 已替换为 `RootMismatchDetected`）相应更新。

## 3. 测试计数（如实填报）

- 全量 **67 个用例**，分布 7 个领域测试文件：`ids / model / operations / validation / atomicity / parent_type / scene_atlas`。
- `test_scene_atlas.cpp` 由 Task 2 原本 **5 个**增至 **27 个**；其中 Back Task 净新增 **22 个**（替换 `EmptyProjectHasOneSceneAndRoot` 为 `SingleSceneContract`，其余为上述 2.1/2.2/2.2b/2.2c/2.3/3 各分组）。
- 原 Task 2 报告中"9 个"为误报，实际为 5 个；本报告以代码为准。

## 4. 新增诊断码（diagnostics.hpp / diagnostics.cpp，未复用、未改含义）

`kSceneCountMismatch`、`kSceneRootInvalid`、`kRootNotUnique`、`kPageNotInAtlas`、`kInvalidTrim`。
（其余复用既有 `kIdKindMismatch`/`kIdDuplicate`/`kInvalidRegionRect`/`kInvalidRotation`/`kTextureAssetNotFound`/`kRegionNotFound`/`kRegionInUse`/`kNodeTypeMismatch`/`kRoot*`。）

## 5. 待确认 / 未决项（非阻塞，沿用既有开放决策）

- `DECISIONS.md` **Q3**：`createImageAsset` 重名策略——当前实现为"拒绝重复名（`kAssetNameDuplicate`）"，若希望"重名复用既有资产"需回填切换。
- `ADR-0004` ID 策略：当前 `adoptData` 与 `adoptImageAssetAsWholeImageRegion` 用随机 ULID（ADR 暂定方案），确定性 ID 留待 Phase 2。
- 构建目录：`build_t4` 为本次验收目录；项目文档约定名为 `build`，可后续清理 `build`/`build_t2`/`build_t3` 等历史目录。
