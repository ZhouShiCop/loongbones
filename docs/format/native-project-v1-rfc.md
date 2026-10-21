# RFC：原生项目格式 Schema v1（`loongbones.native-project`）

- 状态：**阶段 A 已审阅；RFC v1.2 复审通过；阶段 B 已获准启动（实现前须完成 RapidJSON vendor 与许可证登记）**
- 日期：2026-10-20
- 修订：2026-10-20 v1.1（审阅意见：①未知字段的持有者与数字精度 ②诊断码归属 Format 层 ③版本规则与字段表 ④工具登记）
- 修订：2026-10-20 v1.2（复审意见：⑤宿主锚点改用稳定 ID ⑥未知数值允许只有 raw / 已知数值越界拒绝）
- 依赖：Task 1、Task 2（含 Back Task）；`项目规范.md`；ADR-0002；ADR-0003；ADR-0004；`DECISIONS.md`

> **修订摘要（v1.1，对应负责人 2026-10-20 审阅意见）**
>
> | # | 审阅意见 | 处置 | 落点 |
> |---|---|---|---|
> | 1 | 未知字段无存放位置；`JsonValue` 存 `double` 会改变大整数 | 规定由 **Format 层文档对象**持有扩展字段（Domain `ProjectData` **不新增**扩展成员）；未知数字**逐字节保留原始字面量** | §6.5、§6.6 |
> | 2 | 格式诊断码不应进入 Domain | 改归属 **`loongbones::format::FormatDiagnosticCode`**；Domain 校验失败**保留既有** `DiagnosticCode` | §9 |
> | 3 | `minimumReaderVersion <= formatVersion + 1` 无依据；`regionId` 行错位 | 移除该规则；修正表格列值 | §1.1、§2.4 |
> | 4 | 探针缺 README、未登记 | 已由负责人补 `tools/format-probe/README.md` 并登记 `STRUCTURE.md` / `Agent_Task/Task3.md` | §8.3 |
| 5 | §6.5 的 Pointer 单独不能确定归属（重排后会挂到别的节点） | 宿主锚点改为 **`(kind, 稳定 ID)`**；无 ID 容器用**最近的 ID 祖先 + 相对子路径**；编码时才解析为当前 Pointer | §6.5 |
| 6 | §6.6 的 `1e400` 无法表示为 int64/uint64/double | **未知数值允许只有 raw、不做类型化转换**；**已知数值才按字段类型解析，越界时拒绝**（新增 `kJsonNumberOutOfRange`） | §6.6、§9.3 |
>
> 待决项 U1–U5 已于 2026-10-20 由负责人决定，见 **§11**。
- 目标项目：`D:\Works\Spine\Loongbones`
- 配套文件：
  - 机器可读约束：[`native-project-v1.schema.json`](native-project-v1.schema.json)
  - 合法 / 非法 fixture：[`fixtures/`](fixtures/)
  - 可运行探针：[`../../tools/format-probe/`](../../tools/format-probe/)

## 0. 摘要与范围声明

本文冻结 Loongbones 原生格式的首版契约（v1），目标是让**任何人仅凭本文 + 配套 schema + fixture 即可编写独立的读写器**，且与当前领域模型零分叉。

**v1 只持久化当前已实现的静态模型**：Project、Scene、Root/Group/Image(Sprite) Node、ImageAsset、TextureAsset、Atlas、AtlasPage、AtlasRegion、Transform、节点层级与默认 Scene。

**v1 明确不包含（不得伪造空模型或临时字段预占格式）**：

| 不在 v1 的对象 | 原因 / 归属 |
|---|---|
| Skeleton / Bone / Slot / Attachment | 属骨骼动画领域模型，另行立项 |
| Animation / Timeline / Curve | 同上 |
| IK / Transform 约束 / Path 约束 | 同上 |
| Spine / DragonBones 导入与解析 | 属导入器专项（Task 4 及以后），本格式**不冒充**任何第三方格式 |
| 求值结果、渲染缓存、GUI 状态、自动保存元数据 | 运行时数据，规范 §3.3 禁止写入 v1 |

阶段 A **未写入 codec、未修改领域模型**，也未引入任何第三方依赖。§7 的 JSON 库选型与 §9 的诊断码分类已由负责人于 2026-10-20 决定（见 §11），在**阶段 B 获批后**才生效并落地（含 vendor RapidJSON 与登记许可证）。

---

## 1. 文档顶层结构与元数据

### 1.1 顶层对象

文档根必须是 JSON 对象（RFC 8259），UTF-8 无 BOM，键顺序无语义（但写出时按固定顺序，见 §4.5）。

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|---|---|---|---|---|
| `formatId` | string | 是 | — | 固定 `"loongbones.native-project"`。不匹配必须拒绝，不得猜测或降级 |
| `formatVersion` | integer | 是 | — | 本 v1 恒为 `1`。不含小数/字符串 |
| `minimumReaderVersion` | integer | 是 | — | 能正确读取本文档所需的最低 Reader 版本。v1 为 `1` |
| `appVersion` | string | 否 | 省略 | 写出本文档的应用版本。与 `formatVersion` 分离（规范 §4）。**写出时提供，读取时可省略，且不参与任何格式兼容判定**（U4） |
| `capabilities` | object | 否 | 省略 | 能力/扩展声明，见 §1.2 |
| `project` | object | 是 | — | `ProjectData` 的完整内容，见 §2 |

**格式版本与 Reader 版本的关系**：

- Reader 支持区间为 `[1, kReaderMaxVersion]`，当前 `kReaderMaxVersion = 1`。
- 若 `minimumReaderVersion > kReaderMaxVersion` → **拒绝**（`FORMAT_TOO_NEW`），不得"打开后保存并悄悄丢字段"（规范 §3.3）。
- 若 `formatVersion > kReaderMaxVersion` → **拒绝**（`FORMAT_VERSION_UNSUPPORTED`）。
- 若 `formatVersion < 1` → **拒绝**（`FORMAT_VERSION_UNSUPPORTED`）。

> **已移除的规则**：早期草案曾要求 `minimumReaderVersion <= formatVersion + 1`。该约束缺乏充分依据（写者完全可以要求一个比格式版本更新的 Reader 来修复读取侧缺陷），且会人为限制演进，故**不再作为格式契约**。v1 只保留上面三条拒绝规则。

### 1.2 能力 / 扩展声明

```json
"capabilities": {
  "required": ["loongbones.atlas.v1"],
  "optional": ["loongbones.experimental.foo"]
}
```

| 字段 | 类型 | 必需 | 说明 |
|---|---|---|---|
| `required` | string[] | 否 | **必需能力**。出现 Reader 不支持的条目 → 拒绝打开（`CAPABILITY_UNSUPPORTED`） |
| `optional` | string[] | 否 | **可选扩展**。不认识的条目可安全忽略，但内容仍须保留（见 §6） |

- 能力标识符使用 `点分小写` 命名空间，前缀 `loongbones.` 保留给官方。
- v1 基线自带能力无需列入 `required`；`required` 只用于**超出 v1 基线**的必需能力。v1 文档通常省略 `capabilities`。
- 未知必需能力 → 拒绝；**不得**打开后保存时静默删除（规范 §3.3）。

### 1.3 顶层 → `ProjectData` 映射

| JSON 路径 | Domain 成员 | 备注 |
|---|---|---|
| `project.id` | `ProjectData::id` (ProjectId) | 顶层 `projectId` **不重复出现**，避免双份真值 |
| `project.name` | `ProjectData::name` | |
| `project.rootNodeId` | `ProjectData::rootNodeId` | 冗余兼容字段，见 §2.2 |
| `project.defaultSceneIndex` | `ProjectData::defaultSceneIndex` | 单场景契约下恒为 `0` |
| `project.scenes[]` | `ProjectData::scenes` | 恰好 1 项 |
| `project.nodes[]` | `ProjectData::nodes` | 与 `nodeOrder` 平行同序 |
| `project.nodeOrder[]` | `ProjectData::nodeOrder` | 保序 |
| `project.assets[]` | `ProjectData::assets` | ImageAsset |
| `project.textures[]` | `ProjectData::textures` | TextureAsset |
| `project.atlases[]` | `ProjectData::atlases` | Atlas |

> 顶层只放 `formatId`；`projectId` 只出现在 `project.id`，消除"两份 ID 不一致"的可能。

---

## 2. 持久化字段清单

命名统一 `camelCase`；枚举用**稳定字符串**（不序列化 C++ 枚举整数）；数组顺序有语义的**必须保序**。

### 2.1 `project`（ProjectData）

| 字段 | 类型 | 单位 | 默认值 | 可空 | 必需 | 说明 |
|---|---|---|---|---|---|---|
| `id` | string | — | — | 否 | 是 | ProjectId，26 字符 ULID |
| `name` | string | — | — | 否 | 是 | 工程展示名 |
| `rootNodeId` | string | — | — | 否 | 是 | 与 `scenes[0].rootNodeId` 必须相等（§2.2） |
| `defaultSceneIndex` | integer | — | `0` | 否 | 是 | 单场景契约下必须为 `0` |
| `scenes` | array[Scene] | — | — | 否 | 是 | 长度必须恰为 `1` |
| `nodes` | array[Node] | — | — | 否 | 是 | |
| `nodeOrder` | array[string] | — | — | 否 | 是 | 与 `nodes` 长度相同、内容一一对应 |
| `assets` | array[ImageAsset] | — | — | 否 | 是 | |
| `textures` | array[TextureAsset] | — | — | 否 | 是 | |
| `atlases` | array[Atlas] | — | — | 否 | 是 | |

### 2.2 冗余兼容字段与一致性校验（Root / 单 Scene）

当前模型存在两处表达同一事实的冗余：`ProjectData::rootNodeId` 与 `Scene::rootNodeId`。

**编码规则**：

1. **写出时两者都必须写入**，且值相同。
2. **读入时两者都必须存在**（缺失 → `MISSING_REQUIRED_FIELD`）。
3. **一致性校验**（Domain `validate()` 已实现，对应诊断码 `kSceneRootInvalid`）：
   - `scenes.size() == 1`，否则 `kSceneCountMismatch`；
   - `defaultSceneIndex == 0`，否则 `kSceneNotFound`；
   - `scenes[0].rootNodeId` 必须指向存在的节点，否则 `kSceneRootInvalid`；
   - 该节点 `type == "root"`，否则 `kSceneRootInvalid`；
   - `project.rootNodeId == scenes[0].rootNodeId`，否则 `kSceneRootInvalid`。
4. **全工程恰好一个 `type == "root"` 的节点**，否则 `kRootNotUnique`。
5. 根节点必须：`parent == null`、恒等变换（`x=y=rotationDeg=0`、`scaleX=scaleY=1`）、`assetId == null` 且 `regionId == null`。

**未来处理**：多场景与"每个场景一棵树"的语义属后续 RFC。届时应**新增** `scenes` 多项能力与新字段，而不是改变 `rootNodeId` 的现有含义（只增不改原则，§5）。

### 2.3 `scenes[]`（Scene）

| 字段 | 类型 | 默认值 | 可空 | 必需 | 说明 |
|---|---|---|---|---|---|
| `id` | string | — | 否 | 是 | SceneId（ULID） |
| `name` | string | — | 否 | 是 | |
| `rootNodeId` | string | — | 否 | 是 | NodeId，指向 `nodes[]` 中 `type == "root"` 的节点 |

### 2.4 `nodes[]`（NodeRecord）

`NodeCommon` 被**扁平化**到节点对象内（不额外嵌套 `common` 层），因为 `common` 是 C++ 实现细节，不是格式语义。

| 字段 | 类型 | 单位 | 默认值 | 可空 | 必需 | 说明 |
|---|---|---|---|---|---|---|
| `id` | string | — | — | 否 | 是 | NodeId（ULID） |
| `name` | string | — | — | 否 | 是 | |
| `parent` | string \| null | — | `null` | 是 | 是 | 场景根必须为 `null`；其余节点必须指向存在的父节点 |
| `children` | array[string] | — | `[]` | 否 | 是 | **有语义顺序**，即该节点下的绘制/遍历顺序；不得含重复项 |
| `transform` | object | 见 §2.5 | 恒等 | 否 | 是 | 局部变换 |
| `visible` | boolean | — | `true` | 否 | 否 | |
| `locked` | boolean | — | `false` | 否 | 否 | 编辑期语义，不影响几何/绘制 |
| `type` | string enum | — | — | 否 | 是 | `"root"` \| `"group"` \| `"image"` |
| `assetId` | string \| null | — | `null` | 是 | 否 | ImageAssetId；仅 `type == "image"` 可持 |
| `regionId` | string \| null | — | `null` | 是 | 否 | AtlasRegionId；仅 `type == "image"` 可持 |

**互斥约束（校验规则 9，Domain `typeMatchesData()` 已实现）**：

- `type == "image"`：`assetId` 与 `regionId` **恰好其一非空**（XOR），否则 `kNodeTypeMismatch`。
- `type == "root"` 或 `"group"`：`assetId` 与 `regionId` **都必须为空**，否则 `kNodeTypeMismatch` / `kGroupCannotHoldAsset`。

### 2.5 `transform`（Transform）

| 字段 | 类型 | 单位 | 默认值 | 可空 | 必需 |
|---|---|---|---|---|---|
| `x` | number | 工程空间单位 | `0.0` | 否 | 否 |
| `y` | number | 工程空间单位 | `0.0` | 否 | 否 |
| `rotationDeg` | number | 度（°） | `0.0` | 否 | 否 |
| `scaleX` | number | 倍率 | `1.0` | 否 | 否 |
| `scaleY` | number | 倍率 | `1.0` | 否 | 否 |

- 坐标系 **Y-up**、旋转单位 **度**、组合顺序 `world = parent × local`（ADR-0003 C1/C2/C9）。
- 所有数值必须是**有限数**；`NaN` / `Inf` 非法（校验规则 10）。JSON 本身无 `NaN`/`Inf` 字面量，任何非 RFC 8259 数值扩展一律**拒绝**（见 §4.1）。

### 2.6 `assets[]`（ImageAsset）

| 字段 | 类型 | 单位 | 默认值 | 可空 | 必需 | 说明 |
|---|---|---|---|---|---|---|
| `id` | string | — | — | 否 | 是 | ImageAssetId |
| `name` | string | — | — | 否 | 是 | 工程内唯一（重复 → `kAssetNameDuplicate`） |
| `type` | string enum | — | `"static"` | 否 | 否 | 仅 `"static"` |
| `storage` | string enum | — | `"external"` | 否 | 是 | `"external"` \| `"inProject"` |
| `location` | string | — | — | 否 | 是 | 位置标识，见 §3 |
| `pixelWidth` | integer \| null | 像素 | `null` | 是 | 否 | |
| `pixelHeight` | integer \| null | 像素 | `null` | 是 | 否 | |

### 2.7 `textures[]`（TextureAsset）

| 字段 | 类型 | 单位 | 默认值 | 可空 | 必需 |
|---|---|---|---|---|---|
| `id` | string | — | — | 否 | 是 |
| `name` | string | — | — | 否 | 是 |
| `storage` | string enum | — | `"external"` | 否 | 是 |
| `location` | string | — | — | 否 | 是 |
| `pixelWidth` | integer \| null | 像素 | `null` | 是 | 否 |
| `pixelHeight` | integer \| null | 像素 | `null` | 是 | 否 |

### 2.8 `atlases[]`（Atlas）

| 字段 | 类型 | 必需 | 说明 |
|---|---|---|---|
| `id` | string | 是 | AtlasId |
| `name` | string | 是 | |
| `pages` | array[AtlasPage] | 是 | 保序 |
| `regions` | array[AtlasRegion] | 是 | 保序 |

#### 2.8.1 `pages[]`（AtlasPage）

| 字段 | 类型 | 单位 | 默认值 | 可空 | 必需 |
|---|---|---|---|---|---|
| `id` | string | — | — | 否 | 是 |
| `name` | string | — | — | 否 | 是 |
| `textureAssetId` | string | — | — | 否 | 是 |
| `width` | integer | 像素 | — | 否 | 是（> 0） |
| `height` | integer | 像素 | — | 否 | 是（> 0） |

#### 2.8.2 `regions[]`（AtlasRegion）

| 字段 | 类型 | 单位 | 默认值 | 可空 | 必需 | 说明 |
|---|---|---|---|---|---|---|
| `id` | string | — | — | 否 | 是 | AtlasRegionId（ULID） |
| `name` | string | — | — | 否 | 是 | |
| `pageId` | string | — | — | 否 | 是 | 必须属于**同一 Atlas** |
| `packedX` | integer | 像素 | `0` | 否 | 否 | 打包矩形左上角 |
| `packedY` | integer | 像素 | `0` | 否 | 否 | |
| `packedW` | integer | 像素 | `0` | 否 | 是（> 0） | 打包矩形尺寸（**精灵自身坐标系**） |
| `packedH` | integer | 像素 | `0` | 否 | 是（> 0） | |
| `rotation` | string enum | — | `"none"` | 否 | 否 | `"none"` \| `"90"` \| `"180"` \| `"270"` |
| `originalW` | integer | 像素 | `0` | 否 | 是（> 0） | 未裁切原始尺寸 |
| `originalH` | integer | 像素 | `0` | 否 | 是（> 0） | |
| `trimmedW` | integer | 像素 | `0` | 否 | 是（> 0） | 裁切后尺寸，**方向与 `original` 一致** |
| `trimmedH` | integer | 像素 | `0` | 否 | 是（> 0） | |
| `offsetX` | integer | 像素 | `0` | 否 | 否 | trim 偏移，**方向与 `original` 一致**，≥ 0 |
| `offsetY` | integer | 像素 | `0` | 否 | 否 | |

**方向约定（权威表述，与 `domain/include/loongbones/domain/atlas.hpp` 一致）**：

- **打包旋转只影响页内实际占位**：`rotation ∈ {"90","270"}` 时，页内占位尺寸转置为 `(packedH, packedW)`。
- **trim 与 offset 不受打包旋转影响**：它们是在原始图像上定义的，方向与 `originalW/H` 一致，**不做转置**。
- 由此，合法性判据为：

```
占用宽 = (rotation ∈ {90,270}) ? packedH : packedW
占用高 = (rotation ∈ {90,270}) ? packedW : packedH
packedX + 占用宽 <= page.width
packedY + 占用高 <= page.height
offsetX + trimmedW <= originalW
offsetY + trimmedH <= originalH
```

> 该约定在 Task 2 Back Task 中曾出现代码与注释不一致的实现缺陷（trim 被错误转置），已修正并补旋转回归用例。v1 格式以本条为唯一权威口径。

### 2.9 `nodeOrder[]`

- 与 `nodes` 长度相同、内容一一对应，**顺序有语义**（决定遍历顺序）。
- 缺失、长度不符或内容不匹配 → `MISSING_REQUIRED_FIELD` / `DOMAIN_VALIDATION_FAILED`。

---

## 3. 资源位置字段语义（`location` + `storage`）

两个存储模式（`StorageMode`）都必须能表达，且**语义不含混**：

| `storage` | `location` 语义 | 可移植性 |
|---|---|---|
| `"external"` | 指向工程**外部**图片文件的引用 | **不可**假定可移植；通常是本机路径 |
| `"inProject"` | 指向工程**内部**资源（相对工程目录/资源根） | 可移植 |

规则：

1. `location` 是**不透明的位置标识**：v1 **不解析、不校验存在性、不解码、不复制**图片文件（规范 §3.3、Task 1 §3.5）。
2. **不得把机器绝对路径当作可移植路径**：写出时如实保留原值；读入时如实保留原值。是否相对化属"资源重定位"功能，**不在本 Task**（§4.1 已明确排除）。
3. 写出 `location` 时**不得静默重写**（例如自动改成相对路径、自动修正分隔符）。分隔符保持原样。
4. `storage` 与 `location` 的对应关系由**写者**保证；读入端只做枚举合法性校验，不做启发式推断。
5. 空 `location` 允许（缺省值），但 `storage == "inProject"` 时通常应非空——**v1 不做强制**，交由上层资源模块。

---

## 4. JSON 数值与文档规则

### 4.1 数值规则

- 严格遵循 RFC 8259 数值文法：
  `-? (0 | [1-9][0-9]*) ( \. [0-9]+ )? ( [eE] [+-]? [0-9]+ )?`
- **拒绝**以下所有形式（属非标准扩展，常见于 Unity/Spine JSON，属**导入器**职责，**不得**进入本原生格式）：

| 非法形式 | 示例 |
|---|---|
| `NaN` / `Infinity` / `-Infinity` | `{"x": NaN}` |
| 前导零 | `{"x": 01}` |
| 显式正号 | `{"x": +1}` |
| 缺整数位的小数 | `{"x": .8}` |
| 十六进制 | `{"x": 0x10}` |
| 空小数 / 空指数 | `{"x": 1.}`、`{"x": 1e}` |
| 尾随小数点后无数字 | `{"x": 1.}` |

- 诊断码：`JSON_SYNTAX_ERROR`（词法层面）/ `NON_FINITE_NUMBER`（遇到 `NaN`/`Infinity` 字面量时给出更明确的码）。
- **写出**：浮点数用能精确保持往返的最短表示（C++17 `std::to_chars` 语义），不输出 `NaN`/`Inf`（领域校验已保证不会出现）。
- **精度分工**：**已知字段**按类型化值重新序列化（保证 §4.5 的确定性）；**未知字段**的数值必须逐字节回写原始字面量，不经 `double` 舍入 —— 见 §6.6。
- **合法字面量但越界**（如 `1e400`、超 `int64` 的整数）：
  - **已知字段** → **拒绝打开**（`JSON_NUMBER_OUT_OF_RANGE`）；
  - **未知字段** → 只保留原始字面量，**不是错误**（§6.6.3）。

### 4.2 重复 key

- 对象中重复 key **必须拒绝**（`DUPLICATE_KEY`），**不得**静默取最后一个。
- 理由：重复 key 是数据损坏或恶意构造的强信号；静默取最后一个会让"文件看起来能开"但语义不明，违反规范 §3.3 的"不得悄悄删字段"。
- 位置：报告重复 key 的 JSON Pointer 路径与字节偏移。

### 4.3 字符编码

- 必须是**合法 UTF-8**（拒绝非法序列、过长编码、孤立代理项）。
- 拒绝 UTF-8 BOM（可作为顶层第一个字符显式检出并报错）。
- `\u` 转义的代理对必须成对；孤立代理 → `INVALID_UTF8`。
- 控制字符 U+0000–U+001F 必须转义（裸出现 → `JSON_SYNTAX_ERROR`）。

### 4.4 深度 / 大小上限（Reader 可配置安全上限，U5）

| 限制 | 默认值 | 诊断码 |
|---|---|---|
| 嵌套深度 | 64 | `DEPTH_LIMIT_EXCEEDED` |
| 文档字节数 | 64 MiB | `SIZE_LIMIT_EXCEEDED` |
| 字符串长度（字符串值、对象键名、数值字面量原文） | 1 MiB | `SIZE_LIMIT_EXCEEDED` |

**数值字面量长度**：`maxStringBytes` 同样约束数值字面量的**原始**长度。检查发生在
任何"等长占位"替换**之前**（占位后重解析时回调只会看到 `0`，届时再检查就来不及了）；
超限即拒绝打开，并给出 JSON Pointer 与字节偏移（§4.6）。

**越界数值的个数**：本格式**不**对"语法合法但超出 double 量级的数值"（如 `1e400`）
的个数设置独立上限。其处理能力完全由上表的文档字节数上限自然约束：字面量互不相交且
各占至少 1 字节，故任何合法文档的所需处理量都随文档大小线性增长，不可能因某个内部
常量而被拒绝。

**定位（负责人 2026-10-20 决定，U5）**：

- 这三项是 **Reader 侧可配置的安全上限**，用于防御畸形/恶意输入导致的栈溢出与内存放大，**不是持久化 Schema 的一部分**。
- 因此：
  - 上限值**不写入文档**、**不参与格式兼容判定**、**不随 `formatVersion` 演进**；
  - 放宽或收紧上限**不产生**新的格式版本需求，也**不需要**迁移；
  - 默认值（64 / 64 MiB / 1 MiB）只是建议起点，由调用方通过 Reader 配置覆盖。
- 上限被突破时**拒绝打开**并给出诊断，**不得**尝试"部分解析后继续"。

```cpp
// 草案（阶段 B 落地于 format/）
struct ReaderLimits {
  std::size_t maxDepth      = 64;
  std::size_t maxDocBytes   = 64u * 1024u * 1024u;
  std::size_t maxStringBytes = 1u * 1024u * 1024u;
};
```

### 4.5 确定性输出（写出）

- 对象键按 **schema 声明顺序**输出（即本文各表格的行序），不按字典序、不按插入序。
- 数组按领域顺序保序输出（`scenes`、`nodes`、`nodeOrder`、`assets`、`textures`、`atlases`、`pages`、`regions`、`children`）。
- 缩进：2 空格；换行：`\n`；文件末尾保留单个换行。
- 同一 `ProjectData` 连续两次保存，字节内容**完全一致**（临时文件名随机不影响此要求）。

### 4.6 错误定位

所有诊断必须携带位置，格式为 **JSON Pointer**（RFC 6901），例如：

| 场景 | location |
|---|---|
| 顶层缺 `project` | `/project` |
| 第 3 个节点 `type` 非法 | `/project/nodes/2/type` |
| 第 2 个图集第 1 个区域的 `packedW` 越界 | `/project/atlases/1/regions/0/packedW` |
| 重复 key `id`（第 5 个节点） | `/project/nodes/4/id` |

当错误发生在解析阶段（尚未形成完整树）时，退化为**已确定的最近前缀 + 字节偏移**，格式为 `<pointer>@<offset>`，例如 `/project/nodes/4@1207`。

---

## 5. 兼容规则与迁移

### 5.1 版本表示

- v1 文档：`formatVersion = 1`、`minimumReaderVersion = 1`。
- 两者均为**整数**，不是字符串、不是语义化版本号。
- **`formatVersion` 是持久化契约，`appVersion` 只是写出者记录**（规范 §4）。
- **`appVersion` 的契约（U4）**：**写出时提供**（便于排障），**读取时可省略**（缺失**不得**报错），且**不参与任何格式兼容判定** —— 它不影响是否拒绝打开、不进入 `minimumReaderVersion` 比较、不参与迁移键。

### 5.2 只增不改原则

进入 v1 后，schema 演进只允许：

1. **新增可选字段**（有默认值，旧 Reader 可安全忽略）；
2. **新增枚举值**——仅当该枚举用于**可选扩展**时。若用于核心必需语义，必须随 `minimumReaderVersion` 提升并附迁移；
3. **新增能力标识符**（`capabilities.required` / `optional`）。

禁止：删除字段、改变字段类型或单位、改变既有枚举字符串的含义、改变数组顺序语义。

### 5.3 旧 Reader 的行为

| 情况 | 行为 |
|---|---|
| `minimumReaderVersion > 本 Reader 上限` | **拒绝打开**，`FORMAT_TOO_NEW` |
| `formatVersion > 本 Reader 上限` | **拒绝打开**，`FORMAT_VERSION_UNSUPPORTED` |
| `capabilities.required` 含未知项 | **拒绝打开**，`CAPABILITY_UNSUPPORTED` |
| 未知可选字段 | 保留并原样写出（§6） |
| 未知可选能力 | 忽略，但保留声明文本 |

**关键要求**：任何拒绝都必须在**打开时**发生。不得"打开成功 → 用户编辑 → 保存 → 字段静默丢失"（规范 §3.3）。

### 5.4 迁移注册与执行顺序

- 迁移以 `(fromVersion, toVersion)` 为键注册，例如 `1 → 2`。
- 执行顺序：**严格递增、逐版本串行**，不允许跨版本跳跃（v1→v3 必须经由 v1→v2→v3）。
- 每次迁移是**纯函数**：`ProjectData(v_n) → ProjectData(v_{n+1})`，不依赖外部状态、不依赖当前时间戳、不引入随机 ID。
- **幂等性**：对同一输入重复执行同一迁移，输出必须一致（由"纯函数 + 不引入随机性"保证）。
- **失败处理**：迁移作用于**候选副本**；任一步失败即整体放弃，**源文件字节保持不变**，并给出带位置的可定位诊断。
- **回滚策略**：因为只在副本上操作，回滚 = 丢弃副本，天然无需"反向迁移"。

### 5.5 当前无迁移输入（如实声明）

- v1 是本格式的**首个版本**。
- **不存在真实的历史格式或旧版本文件**，因此当前**没有任何实际迁移输入**。
- 阶段 A/B **不得发明无依据的数据转换**（例如假装存在 v0 并编写 v0→v1 转换）。
- 本文仅提供**迁移接口/策略**与 v1 fixture；**第一条真实历史迁移留待真实版本演进时**再编写。

### 5.6 新增版本时的迁移模板

将来引入 v2 时，按下列模板登记（此处为**接口占位，不实现具体转换**）：

```cpp
// 迁移接口（草案，待 RFC 批准后落地于 format/）
struct Migration {
  int fromVersion;
  int toVersion;
  // 纯函数：只依赖输入，不读时钟、不用随机数、不碰文件系统
  ProjectData (*apply)(const ProjectData& in, MigrationDiag* diag);
};

// 注册（示例，v2 出现时填写）
// registry.add({1, 2, &migrate_v1_to_v2});
```

配套要求（v2 真实出现时）：

1. 新增 `formatVersion = 2` 的合法 fixture；
2. 保留 v1 fixture 作为迁移输入；
3. 迁移前后做**语义比较**（字段、顺序、引用、可选值），而非字节比较；
4. 在 `docs/format/` 下记录该迁移的变更说明。

---

## 6. unknown-field 保留机制

### 6.1 目标：语义保留

- 定义：**语义保留** —— 未知键对应的**解析后 JSON 值语义**（null / bool / number / string / array / object，含嵌套结构）在读入→编辑已知字段→写出的全流程中保持不变。
- **不要求**：空白、缩进、键顺序、数字字面量形式的字节级不变（例如 `1.0` 与 `1` 视为同一数值）。
- **归属要求**：未知字段必须**跟随其宿主对象**（节点 / 区域 / 资源等）移动 —— 重排数组后仍归属**原对象**，不得挂到新位置上的别的对象（§6.5.1）。
- **数值要求**：未知数值按其**原始字面量**保留，不要求可被类型化表示（§6.6）。

### 6.2 保留机制

- 每个**已知对象**（顶层、`project`、各节点、各资源、各图集 / 页 / 区域）在解码时，除映射已知键外，额外收集**未被识别的键**到该对象的 "unknown" 容器（键值对，值为完整的 `JsonValue` 子树）。该容器**按 `(kind, 稳定 ID)` 锚点索引，不按位置索引**（§6.5.1）。
- 编码时，先输出已知键（按 schema 顺序），再输出该对象的 unknown 键（按首次出现顺序）。
- 由此，未知可选字段可随数据经过任意次"打开→改已知字段→保存"往返而被原样保留。

### 6.3 未知**必需**扩展的标识与拒绝

- 必需能力通过 `capabilities.required[]` **显式声明**（不使用"未知字段即必需"的隐式推断）。
- 未知必需能力 → **拒绝打开**（`CAPABILITY_UNSUPPORTED`），并列出不支持的能力项。
- 未在 `capabilities.required` 中声明的未知键一律视为**可选**，按 §6.2 保留。
- 这样既满足"未知必需能力不得静默丢失"，也避免把任何未知字段都误判为必需而拒绝合法文档。

### 6.4 保留的边界

- 未知字段**不参与**领域校验与求值（Domain 不知道其语义）。
- 若未知字段与已知字段**同名冲突**（不可能，因为同名即为已知），不适用。
- 未知字段的保留**不构成**对其含义的承诺：写出端保证"原样保留"，不保证"理解"。

---

### 6.5 扩展字段的持有者（审阅意见 1：规定由哪个文档对象持有）

**决定：未知字段由 Format 层的文档对象持有，`ProjectData`（Domain）不新增任何扩展数据成员。**

理由：规范明确禁止 Domain 依赖 JSON 库、Qt、文件系统或格式版本细节。若把扩展数据塞进 `ProjectData`，等于让 Domain 承载"未识别的 JSON 子树"这一纯格式概念，违反依赖方向（Format → Domain，单向）。

#### 6.5.1 宿主锚点：**`（kind, 稳定 ID）`**（复审意见 5）

> v1.1 曾用「所属对象的 JSON Pointer」作索引键。这是**错误的**：Pointer 是位置性的。
> 反例（复审给出）：未知字段原在 `/project/nodes/3`；重排节点后，3 号位上是**另一个**节点，
> 旧 Pointer 会把该字段挂到错误的宿主上 —— 而"按当前顺序重新生成 Pointer"恰恰是**找不到原宿主**时才说的空话。

**锚点规则**：

| 宿主类型 | 锚点 |
|---|---|
| 有稳定 ID 的对象（node / scene / asset / texture / atlas / page / region / project） | **`(kind, id)`** |
| 无 ID 的容器（如 Node 下的 `transform`） | **最近的有 ID 的祖先**的 `(kind, id)` + **相对子路径** |
| 无 ID 但无 ID 祖先的顶层容器（如 `capabilities`） | 特殊 `Document` 锚点（`kind=Document`，`id` 为空）+ 从文档根开始的相对子路径 |
| 顶层文档对象 | 特殊 `Document` 锚点（`kind=Document`，`id` 为空），`subPath` 为空 |

**为什么 ID 锚点可用（已核实）**：

- schema 中 8 种数组对象的 `id` **全部必需**且为 `#/definitions/ulid`；
- Domain **按种类**保证 ID 唯一：`kIdDuplicate` 覆盖 node / asset / texture / atlas / page / region，`kSceneDuplicate` 覆盖 scene；其中页与区域是**全局唯一**。
- 因为唯一性只在**同类内**成立，锚点**必须包含 `kind`** —— `(kind, id)` 才是唯一键，单用 `id` 不够。

**持有者形状（草案，阶段 B 落地于 `format/`）**：

```cpp
enum class OwnerKind {
  Document,  // 顶层文档对象（唯一，无 ID）
  Project, Scene, Node, ImageAsset, TextureAsset, Atlas, AtlasPage, AtlasRegion
};

// 宿主锚点：kind + 稳定 ID（位置无关）
struct OwnerAnchor {
  OwnerKind   kind;
  std::string id;   // Document 时为空
};

// 一条未知字段记录
struct UnknownRecord {
  OwnerAnchor owner;
  std::string subPath;   // 宿主内部到目标容器的 JSON Pointer；"" 表示宿主对象本身
  std::vector<std::pair<std::string, JsonValue>> entries;  // 首次出现顺序
};

// 索引键 = (kind, id, subPath) —— 不含任何位置信息
using UnknownKey       = std::tuple<OwnerKind, std::string, std::string>;
using UnknownFieldIndex = std::map<UnknownKey, UnknownRecord>;

struct ProjectDocument {
  ProjectData       data;      // 已通过 Project::adoptData() 完整校验
  UnknownFieldIndex unknowns;  // 旁路保留的未知字段，与 data 平行
};
```

**编码时的解析**（ID 锚点 → 当前 Pointer）：

```
Document                        -> ""
Project                         -> "/project"
Scene / Node / Asset / ...      -> 在 ProjectData 中按 kind 查 id，取其当前数组下标
pointer = resolve(owner) + subPath
```

**解析示例**：

| 未知字段来源 | 锚点 | `subPath` | 重排后编码位置 |
|---|---|---|---|
| 顶层 `vendorNote` | `Document` | `""` | 顶层 |
| `/project/customTag` | `(Project, 01J…000)` | `""` | `/project` |
| `/project/nodes/3/customZ` | `(Node, 01J…003)` | `""` | `/project/nodes/7`（跟着**该节点**走） |
| `/project/nodes/3/transform/flipX` | `(Node, 01J…003)` | `/transform` | `/project/nodes/7/transform` |
| `/project/atlases/0/regions/1/packerMeta` | `(AtlasRegion, 01J…R1)` | `""` | `/project/atlases/0/regions/4` |
| `/capabilities/loongbones.foo` | `Document` | `/capabilities` | `/capabilities` |

**解析失败 = 宿主已删除 → 丢弃该记录**（§6.4：保留的前提是宿主存在）。

> 若将来新增**无 ID** 的对象种类，仍用其「最近的有 ID 的祖先 + 相对子路径」锚定；**任何情况下都不得退回用裸 Pointer 作索引键**。

#### 6.5.2 生命周期（对应 Task 3 §5 验收项「打开 → 修改已知字段 → 保存后仍存在」）

| 阶段 | 动作 | `ProjectData` | `UnknownFieldIndex` |
|---|---|---|---|
| 1 解码 | 解析 JSON → 映射已知键为候选 **+** 按 `(kind, id, subPath)` 收集未知键 | 候选 | 填充 |
| 2 校验 | `Project::adoptData()` 校验候选 | 通过则确定；**失败则整体打开失败** | 通过则**锚点有效性同时得到保证**；失败则整体丢弃（不会出现"半保留"） |
| 3 编辑 | 用户改已知字段，含**重排 / 插入 / 删除** | 变化 | **不动**（锚点是 ID，不受顺序影响） |
| 4 编码 | 解析每个锚点为当前 Pointer → 按 §4.5 输出已知键 → 再按 §6.2 输出未知键（原序） | 读出 | 读出 |

**边界**：新增的对象没有未知记录；被删除的对象其记录因锚点解析失败而丢弃。这是已知且可接受的行为。

> **锚点与 ID 唯一性的耦合**：阶段 2 的 `adoptData()` **保证**了 ID 唯一（重复则以 `kIdDuplicate` / `kSceneDuplicate` 失败）。因此进入阶段 3/4 时锚点必然无歧义；若阶段 2 失败则整体丢弃，**永远不会用无效锚点去解析**。

---

### 6.6 未知数字的精确保留（审阅意见 1 / U3：不得经 `double` 舍入）

**要求**：未知字段中的数值必须**精确保留**，`double` 不得成为其唯一表示。

#### 6.6.1 核心规则（复审意见 6）

> v1.1 要求每个数值节点同时保存 `raw` **和** 类型化 `value`。这对 `1e400` 是不成立的：
> 该字面量语法合法，但**无法**表示为 `int64` / `uint64` / `double` 中任何一个。

**因此明确规定**：

| 规则 | 内容 |
|---|---|
| **未知数值** | **只允许有 `raw`，不做任何类型化转换。** 不解析成 `int64`/`uint64`/`double`，不调用 `strtod`/`from_chars`。只做 RFC 8259 文法校验（不合法 → `kJsonSyntaxError`） |
| **已知数值** | 才按**该字段的 schema 类型**解析：`integer` → `int64`；`number` → `double` |
| **已知数值越界** | 超出目标类型可表示范围 → **拒绝打开**（`kJsonNumberOutOfRange`，新增码见 §9.3） |

**关键推论：未知字段不存在"越界"问题** —— 因为没有类型转换，就没有溢出。

#### 6.6.2 数值节点形状

```cpp
// 草案（阶段 B 落地于 format/）
struct JsonNumber {
  // 逐字节原始字面量：解析期始终可得；未知字段必须保留到编码期
  std::string raw;

  // 类型化值：仅当该数值被当作「已知字段」读取时才解析；未知字段保持 nullopt
  std::optional<std::variant<std::int64_t, std::uint64_t, double>> value;
};
```

#### 6.6.3 处置矩阵

| 位置 | 字面量 | `raw` | `value` | 行为 |
|---|---|---|---|---|
| 未知字段 | `12345678901234567890` | 保留 | **`nullopt`** | 原样回写 `raw` —— **不是错误** |
| 未知字段 | `1e400` | 保留 | **`nullopt`** | 原样回写 `raw` —— **不是错误**（复审意见 6 的核心） |
| 已知 `integer` 字段 | `42` | 保留 | `int64(42)` | 按 §4.5 重新序列化 |
| 已知 `integer` 字段 | `12345678901234567890` | 保留 | 不解析 | **拒绝**，`kJsonNumberOutOfRange` |
| 已知 `number` 字段 | `1.5` | 保留 | `double(1.5)` | 按 §4.5 最短往返表示输出 |
| 已知 `number` 字段 | `1e400` | 保留 | 不解析 | **拒绝**，`kJsonNumberOutOfRange` |
| 任何位置 | `NaN` / `Infinity` | — | — | **语法层拒绝**，`kJsonNonFiniteNumber`（§4.1） |

> `NaN`/`Infinity` 是**非标准字面量**（RFC 8259 不允许），与 `1e400`（**合法字面量但越界**）是两类问题，分别用两个不同的码。

#### 6.6.4 越界检测方式

- **优先**：`std::from_chars`（C++17 `<charconv>`）—— 溢出时返回 `std::errc::result_out_of_range`，**不**把结果写成 `inf`/`HUGE_VAL`，可精确区分"越界"与"合法值"。
- **退化**：若工具链的浮点 `from_chars` 不可用，用 `std::strtod` + `errno == ERANGE` 判越界。
- **禁止**：先转成 `double` 再用 `std::isinf` 事后判断 —— 那会在转换阶段就丢失"原本多大"的信息，且无法区分 `1e400` 与真的 `Infinity` 字面量。

#### 6.6.5 落地要求

**对 RapidJSON**（见 §7.4）：在 SAX 的 `Number(...)` 回调中**同时截取原始 token 文本**存入 `raw`（可用 `kParseNumbersAsStringsFlag` 后自行判定）；**必须**有以下往返测试：

1. 20 位十进制整数（超 `int64`/`uint64`）作为**未知字段**时逐字节回写；
2. `1e400` 作为**未知字段**时逐字节回写，且**不得**触发数值转换或报错；
3. 高精度小数（20 位小数）作为**未知字段**时逐字节回写；
4. 上述字面量作为**已知** `number` 字段时被 `kJsonNumberOutOfRange` 拒绝；
5. 已知字段按类型化值重新序列化，不要求字节一致（§4.5）。

**探针的已知局限（如实声明）**：阶段 A 探针 `tools/format-probe/src/json.hpp` 的 `JsonValue` 把数值存为 `double`，**不满足**本条。探针是**只读**校验工具、不做写出，其 `--selftest` **不证明**数字往返保真。该要求由阶段 B 的 codec 覆盖并测试，不得以探针通过为由宣称本条已验证。

---

## 7. JSON 库选型（已决定：RapidJSON v1.1.0，MIT）

**硬需求（来自规范与本文 §4）**：

| # | 需求 | 说明 |
|---|---|---|
| R1 | Qt-free | 规范 §3.1；禁止向 Domain 引入 Qt |
| R2 | C++17 | ADR-0001 D1 |
| R3 | **重复 key 可检出** | §4.2；多数库"后者覆盖前者" |
| R4 | **带位置的错误诊断**（JSON Pointer / 偏移） | §4.6、Task 3 §4.2 |
| R5 | **未知字段可保留并回写** | §6 |
| R6 | 严格 UTF-8 校验 | §4.3 |
| R7 | 拒绝 `NaN` / `Infinity` 等非标准数值 | §4.1 |
| R8 | 可固定版本、许可证清晰、构建/分发成本低 | 规范 §8.3 |

### 7.1 候选对比

| 候选 | 许可证 | 形态 | R3 重复 key | R4 位置诊断 | R5 未知字段 | R7 严格数值 | 备注 |
|---|---|---|---|---|---|---|---|
| **自研最小严格解析器** | 无（自有） | 约 2 个文件 | **可控** | **可控** | **可控** | **可控** | 需自担维护；约 600–900 行 |
| RapidJSON | MIT（BSD 风格） | 头文件 | 否（后者胜） | 有偏移，无 Pointer | 需自行保留 | 不支持 `NaN`（默认拒绝） | 性能强；DOM 内存占用偏高 |
| nlohmann/json | MIT | 单头文件 | 否（后者胜） | 有异常与位置 | 对象即 map，易保留 | 可拒绝 | API 友好；编译期开销大 |
| simdjson | Apache-2.0 | 静态库 | 否（后者胜） | 有偏移 | 需自行保留 | 严格 | 极快；仅解析、无写出 |
| Boost.PropertyTree | BSL | 库 | 否 | 弱 | 弱 | 弱 | **非真正 JSON**，不推荐 |
| Qt `QJsonDocument` | LGPL/GPL/商业 | 库 | 否 | 弱 | 弱 | 弱 | **违反 R1**，禁止 |

### 7.2 关键结论：R3 是分水岭

- RapidJSON / nlohmann / simdjson **都不把重复 key 视为错误**，默认行为是"后者覆盖前者"。
- 要满足 R3，要么在这些库之上加一层**自定义 SAX / 事件回调**来检测重复 key（可行但要绕过其 DOM 抽象），要么自己控制解析。
- 本项目已有两处"零依赖自研"先例：ULID 编解码（`domain/src/ids.cpp`）与极简测试框架（`tests/domain/test_framework.hpp`）；但负责人已决定走**依赖 + SAX 适配**路线（§7.3），而非自研解析器。
- **结论如何被满足**：§7.3 采用 RapidJSON 的 **SAX 接口**自建值树，把 R3（重复 key 检测）与 §6.6（数字原始字面量）两项硬要求放在我们自己的适配层里解决，从而**不依赖**上游 DOM 的默认行为。

### 7.3 决定：采用 RapidJSON（负责人 2026-10-20，U1）

**决定**：

| 项 | 结论 |
|---|---|
| 库 | **RapidJSON**（Tencent/rapidjson） |
| 版本固定 | release **`v1.1.0`**，commit **`f54b0e47a08782a6131cc3d60f94d038fa6e0a51`**（发布于 2016-08-25） |
| 许可证 | **MIT**（BSD 风格，宽松） —— 已登记于 [`docs/licenses-and-risks.md`](../licenses-and-risks.md) |
| 形态 | 头文件库，无运行时依赖，符合 R1（Qt-free）/ R2（C++17）/ R8 |
| 取源方式 | vendor 固定 commit，**不接受直接跟随 `master`**；取源时同步登记 commit SHA 与 MIT 全文 |

**用法决定（关键）**：**不直接使用 RapidJSON 的 DOM 构建器**，而是基于其 **SAX 接口自建 JSON 值树**：

| 需求 | 落地方式 |
|---|---|
| R3 重复 key | 在 SAX 的 `Key(...)` 回调中于当前对象作用域查重，**首个重复即拒绝** |
| R4 位置诊断 | 记录字节偏移，由路径栈合成 JSON Pointer（§4.6） |
| R5 未知字段 | 自建值树保留完整子树，按 §6.5 收入 `UnknownFieldIndex` |
| R6 严格 UTF-8 | RapidJSON 提供 `kValidateEncodingFlag`；BOM 由我们前置检出 |
| R7 拒绝 `NaN`/`Inf` | RapidJSON 默认即拒绝这些非标准字面量；另需 `kParseFullPrecisionFlag` 并自测 |
| §6.6 数字精度 | 在 `Number(...)` 回调中同时截取**原始 token 文本**存入 `raw`（见 §7.4） |

> **为什么不用 RapidJSON 的 DOM**：其 DOM 对重复 key 的默认行为是"后者覆盖前者"（违反 R3），且其 `GenericValue` 的数值分型会丢失超大整数的精度（违反 §6.6）。走 SAX 自建值树可同时满足两者，代价是约一层适配代码。

### 7.4 阶段 B 落地约束（RapidJSON）

1. **固定 commit**：`f54b0e47a08782a6131cc3d60f94d038fa6e0a51`（`v1.1.0`）。升级须走与引入时相同的评审与登记流程。
2. **隔离**：RapidJSON 只出现在 `format/`（及阶段 B 的 `tools/format-tests/`）的**实现文件**中，**不得**出现在任何公开头文件，更**不得**进入 `domain/`。
3. **数值处理的两条硬规则**（§6.6）：
   - **未知数值只保留 `raw`，不做类型化转换**；
   - **已知数值按字段类型解析，越界则拒绝**。
   必须按 **§6.6.5 的 5 项测试**验证（含 `1e400` 作为未知字段**不得报错**、作为已知字段**必须拒绝**）。
4. **重复 key 测试**：对象内重复 key 必须被 SAX 层检出并拒绝，不得静默取后者。

### 7.5 阶段 A 探针的定位

阶段 A 的自研解析器（`tools/format-probe/src/json.hpp` / `json.cpp`）**继续作为探针存在**，但**不是阶段 B 的正式解析器**：

- 它用于证明"RFC 描述的每条约束都可判定"，并作为 fixture 回归的可执行基线；
- 其 `JsonValue` 把数值存为 `double`（不满足 §6.6），且**只读、不写出**；
- 阶段 B **不得**因复用探针代码而默认冻结其 API 或数据结构；
- 阶段 B 的 codec 落地后，探针与 codec 应共享 fixture，但实现各自独立。

---

## 8. 配套产物

### 8.1 机器可读结构约束

[`native-project-v1.schema.json`](native-project-v1.schema.json) — JSON Schema（draft-07），覆盖本文 §1–§2 的字段、类型、必需性、枚举字符串与引用约束。

> 说明：JSON Schema 无法表达"跨对象引用闭合"（如 `regionId` 必须指向存在区域、`pageId` 必须属于同一图集）与"全局 ID 唯一性"。这些由**探针**与阶段 B 的 codec 在执行时检查（对应 §9 的诊断分层）。

### 8.2 Fixture

| 路径 | 用途 |
|---|---|
| `fixtures/valid/minimal.json` | 最小合法工程：单 Scene + Root |
| `fixtures/valid/full.json` | 完整合法工程：Root/Group/Image(Sprite) 全节点类型、父子顺序、Transform、ImageAsset、Texture/Atlas/Page/Region（含旋转与 trim）、未知可选字段 |
| `fixtures/invalid/syntax-truncated.json` | JSON 语法错误（截断） |
| `fixtures/invalid/format-id-unknown.json` | 未知 `formatId` |
| `fixtures/invalid/version-unsupported.json` | `formatVersion` 高于 Reader 上限 |
| `fixtures/invalid/minimum-reader-too-new.json` | `minimumReaderVersion` 过高 |
| `fixtures/invalid/capability-required-unknown.json` | 未知必需能力 |
| `fixtures/invalid/type-mismatch.json` | 字段类型错误 |
| `fixtures/invalid/missing-required.json` | 缺必需字段 |
| `fixtures/invalid/enum-invalid.json` | 非法枚举字符串 |
| `fixtures/invalid/id-invalid.json` | 非法 ID 格式 |
| `fixtures/invalid/duplicate-key.json` | 重复 key |
| `fixtures/invalid/domain-inconsistent.json` | 领域不变量不一致（区域引用跨图集页） |

所有 fixture 均为**合成样本**，不含任何 Sdorica / Spine 原始资产。

> **未知字段 fixture 的覆盖范围（如实声明）**：`valid/full.json` 含 3 处未知可选字段（顶层 `vendorNote` 字符串、`project.customTag` 整数、区域 `packerMeta` 对象），用于验证"未知字段不影响合法性判定，且能被识别出来"。
>
> 但**未知字段的往返保留**（§6.5）与**未知数字的精确保留**（§6.6）都需要**写出**能力，而阶段 A 探针是**只读**的。因此：
>
> - 当前**不提供**这两项的 fixture；
> - `customTag` 只覆盖小整数；§6.6 要求的**大整数与 `1e400`** 往返测试、以及 §6.5.1 要求的**"重排节点后未知字段仍跟随原节点"**测试，均属**阶段 B** 范围（§6.6.5、§7.4 第 3 条）；
> - 不得以"探针自检 14/14 通过"为由宣称 §6.5/§6.6 已验证。

### 8.3 可运行探针

[`../../tools/format-probe/`](../../tools/format-probe/) — Qt-free C++17 探针，执行严格解析 + v1 契约校验，逐条输出 `code / location / message`。

- 使用说明：[`tools/format-probe/README.md`](../../tools/format-probe/README.md)
- 结构登记：根 [`STRUCTURE.md`](../../STRUCTURE.md) §2.3
- 构建开关：`-DLOONGBONES_BUILD_FORMAT_PROBE=OFF` 可关闭；`ctest -R format_probe_selftest` 可复现自检

阶段 A 要求覆盖的探针场景：合法文档、缺字段、错误类型、未知字段、未知必需能力、格式版本边界。探针自带 `--selftest`，可对 `fixtures/` 全部条目一键复现运行。

> **能力边界**：探针是**只读**的，不写出 JSON。因此 §6.5/§6.6 的"未知字段往返保留 / 未知数字精确保留"**不由探针验证**，由阶段 B 的 codec 覆盖并测试。

---

## 9. 诊断码（审阅意见 2：格式码归 Format 层，Domain 码保持既有）

### 9.1 分层归属（决定）

**格式错误码属于 Format 层，不属于 Domain。** 负责人 2026-10-20 决定：

- 在 `format/` 新增独立枚举 **`loongbones::format::FormatDiagnosticCode`**；
- **禁止**把下表任何一项加入 `domain/include/loongbones/domain/diagnostics.hpp` 的 `DiagnosticCode`；
- Domain 的数据校验失败**继续复用既有 `DiagnosticCode`**，不改含义、不新增格式码。

### 9.2 两层边界

| 层 | 何时发出 | 枚举 | 覆盖范围 | 示例 |
|---|---|---|---|---|
| **Format** | 字节 → 文档树 → `ProjectData` 候选（尚未交给 Domain） | `format::FormatDiagnosticCode` | 语法、类型、编码、结构、版本、能力、上限、IO | `kJsonSyntaxError`、`kFormatMissingField`、`kFormatTooNew` |
| **Domain** | `Project::adoptData()` / `validate()` 校验候选数据 | `domain::DiagnosticCode`（**既有**） | 领域语义不变量 | `kSceneRootInvalid`、`kRootNotUnique`、`kPageNotInAtlas`、`kRegionNotFound`、`kAssetNameDuplicate` |

**执行顺序与互斥性**：

1. Format 层先完整跑完（解析 → 结构 → 版本/能力 → 字段映射）。
2. **Format 任一环节失败即中止，不进入 Domain**，因此一个文档**不会同时**产生两套码。
3. 只有 Format 全部通过、候选 `ProjectData` 构造完成后，才交给 `Project::adoptData()`；Domain 失败时**保留其既有码并携带 `location`**。
4. 未知字段**不参与**任何一层校验（§6.4）。

### 9.3 `FormatDiagnosticCode`（Format 层，阶段 B 按"只追加"落地）

| 枚举项 | 稳定字符串 | 类别 | 触发条件 |
|---|---|---|---|
| `kJsonSyntaxError` | `JSON_SYNTAX_ERROR` | JSON 语法/类型 | 词法或结构不符合 RFC 8259 |
| `kJsonTypeMismatch` | `JSON_TYPE_MISMATCH` | JSON 语法/类型 | 字段类型与 schema 不符 |
| `kJsonInvalidUtf8` | `JSON_INVALID_UTF8` | 编码 | 非法 UTF-8 / 孤立代理项 / BOM |
| `kJsonDuplicateKey` | `JSON_DUPLICATE_KEY` | 结构 | 对象内重复 key |
| `kJsonNonFiniteNumber` | `JSON_NON_FINITE_NUMBER` | 数值 | 出现 `NaN` / `Infinity` **非标准**字面量 |
| `kJsonNumberOutOfRange` | `JSON_NUMBER_OUT_OF_RANGE` | 数值 | **已知字段**的字面量合法但超出其 schema 类型可表示范围（`1e400`、超 `int64` 的整数）。**未知字段不适用**（§6.6） |
| `kJsonDepthLimit` | `JSON_DEPTH_LIMIT` | 上限 | 嵌套深度超限（§4.4，可配置） |
| `kJsonSizeLimit` | `JSON_SIZE_LIMIT` | 上限 | 文档 / 字符串超限（§4.4，可配置） |
| `kFormatIdUnsupported` | `FORMAT_ID_UNSUPPORTED` | 版本/兼容 | `formatId` 不匹配 |
| `kFormatVersionUnsupported` | `FORMAT_VERSION_UNSUPPORTED` | 版本/兼容 | `formatVersion` 越界 |
| `kFormatTooNew` | `FORMAT_TOO_NEW` | 版本/兼容 | `minimumReaderVersion` 高于本 Reader |
| `kCapabilityUnsupported` | `CAPABILITY_UNSUPPORTED` | 版本/兼容 | `capabilities.required` 含未知必需能力 |
| `kFormatMissingField` | `FORMAT_MISSING_FIELD` | 结构 | 缺必需字段 |
| `kFormatWriteFailed` | `FORMAT_WRITE_FAILED` | IO | 写入 / 临时文件 / 替换失败 |
| `kFormatReadFailed` | `FORMAT_READ_FAILED` | IO | 读取失败 |

规则：

- **只追加，不改动既有码含义**（Task 3 §4.2）；枚举项与稳定字符串一一对应，字符串对外发布后即冻结。
- 上表所有码**只出现在 `format/`**，不进入 `domain/`。
- JSON 语法/类型错误必须携带文件与字段位置（§4.6）。
- 上限类（§4.4）是 Reader 可配置策略，诊断码不变。

### 9.4 阶段 A 的探针与本节的关系

阶段 A 探针**未修改** `domain/include/loongbones/domain/diagnostics.hpp`，其源码中的码名是对上表的**临时抄录**（用于证明可判定性）。

> **缺口如实声明**：探针当前把格式码与领域码混在同一个 `const char*` 常量表里，**未按 §9.1/§9.2 分层**。这不影响阶段 A 的"可判定性"结论，但**不得**被视为 §9 分层契约已实现。分层在阶段 B 落地 `format::FormatDiagnosticCode` 时才真正生效。

---

## 10. ADR-0002 的处置说明

**结论：ADR-0002《原生格式容器方案》保持不变，无需补充。**

理由：

| ADR-0002 已定案项 | v1 RFC 的承接方式 |
|---|---|
| JSON 为核心工程数据、图片独立存储 | §1 / §2 —— `project.json` 顶层结构与资源层字段 |
| 开发期目录工程，支持外部引用 + 工程内资源 | §3 —— `storage` / `location` 双模式语义 |
| 未来可加单文件打包，但不得两套互不兼容模型 | §1.1 —— 单一 `project` 内容模型，容器无关 |
| 暂不压缩、不提前实现二进制容器 | §1 —— v1 无压缩、无二进制块 |
| 保留清晰的资源存取接口 | §3 —— `storage` / `location` 抽象形状固定 |
| 当前范围 = 静态 Scene/Node、图片实例、Atlas/Region 元数据；骨骼动画不进入 | §0 —— 显式排除清单 |

ADR-0002 解决的是**容器形态**（JSON / 目录 / 是否压缩打包）；本 RFC 解决的是**文档内部 schema**（字段、类型、单位、引用、兼容）。两者层次不同，无冲突，故 ADR-0002 无需修改。

若后续引入单文件打包或压缩容器，届时**新增 ADR** 描述容器实现，并复用本 RFC 的 schema 作为唯一内容模型。

---

## 11. 待决项处置（负责人 2026-10-20 已决定）

| # | 事项 | **决定** | 落点 |
|---|---|---|---|
| U1 | JSON 库选型 | **RapidJSON**，固定 release `v1.1.0`（commit `f54b0e47a08782a6131cc3d60f94d038fa6e0a51`），**MIT** 已登记。用 **SAX 接口**自建保留未知字段的 JSON 树，同时检测重复 key。**阶段 A 自研解析器继续作为探针，不是阶段 B 的正式解析器** | §7.3–§7.5、`docs/licenses-and-risks.md` |
| U2 | 诊断码 | 接受本 RFC 的分类与稳定字符串，但**放在 Format 层**（`format::FormatDiagnosticCode`），**不放进 Domain**；Domain 校验失败保留既有 `DiagnosticCode` | §9 |
| U3 | 未知字段 | **保留 JSON 值树**（非规范化为字符串）；未知数字**保留精确数值语义，不经 `double` 舍入** → 逐字节保留原始字面量 | §6.5、§6.6 |
| U4 | `appVersion` | **写出时提供，读取时可省略，不参与格式兼容判定** | §1.1、§5.1 |
| U5 | 深度/大小上限（64 / 64 MiB / 1 MiB） | 作为**可配置的 Reader 安全上限**，**不纳入持久化 Schema** | §4.4 |

### 11.1 仍阻塞阶段 B 的事项

| # | 事项 | 状态 |
|---|---|---|
| B1 | **本 RFC v1.2 的复审通过** | **已通过（2026-10-20）；关闭** |
| B2 | RapidJSON vendor 入库（固定 commit + MIT 全文同步） | 待阶段 B 启动时执行，见 §7.4 |

> **阶段 B 已获准启动。** 实现前按 §7.4 完成 RapidJSON 固定 commit 的 vendor 与 MIT 全文登记；不得修改 `domain/`，除非后续 RFC 明确批准领域变更。

---

## 12. 阶段 A 验收自检

| 项 | 状态 |
|---|---|
| 明确列出 v1 完整边界（支持 / 不支持） | §0 |
| 覆盖规格 §3 全部 8 项要求 | §1–§8 |
| 可由他人仅凭 RFC 编写独立读写器 | §1–§5 给出完整字段、类型、单位、枚举、引用与顺序语义 |
| 所有字段均能映射到现有 Domain API / 数据 | §1.3 + §2（逐字段对应 `ProjectData` 及其成员） |
| 不存在未获批的破坏性领域模型改动 | 阶段 A 未修改 `domain/` 任何文件 |
| 机器可读约束 | §8.1 |
| 合法 / 非法 fixture | §8.2 |
| 可复现运行的探针 | §8.3 |
| ADR-0002 处置说明 | §10 |

### 12.1 v1.1 修订自检（对应负责人 2026-10-20 审阅意见）

| # | 审阅意见 | 处置落点 | 自评 |
|---|---|---|---|
| 1 | 未知字段无存放位置；数字经 `double` 会改变大整数 | §6.5 规定由 Format 层 `ProjectDocument` 持有（`ProjectData` 不新增成员）；§6.6 要求未知数字逐字节保留原始字面量 | 已覆盖；并如实声明探针不满足 §6.6 |
| 2 | 格式诊断码不应进入 Domain | §9.1–§9.3 改归 `format::FormatDiagnosticCode`，给出两层边界与执行顺序 | 已覆盖；并声明探针尚未分层 |
| 3 | `minimumReaderVersion <= formatVersion + 1` 无依据；`regionId` 行错位 | §1.1 移除该规则并说明理由；§2.4 修正 `regionId` 行列值 | 已覆盖 |
| 4 | 探针缺 README、未登记 | §8.3；`tools/format-probe/README.md`、根 `STRUCTURE.md` §2.3、`Agent_Task/Task3.md` | 已由负责人补齐 |
| 5 | Pointer 单独不能确定宿主归属（重排后会挂到别的节点） | §6.5.1 锚点改为 **`(kind, 稳定 ID)`**；无 ID 容器用最近的 ID 祖先 + 相对子路径；编码时才解析为 Pointer；解析失败即丢弃 | 已覆盖；**已核实** 8 类对象 ID 均必需且 Domain 按种类保证唯一，故锚点必须与 `kind` 组合 |
| 6 | `1e400` 无法表示为 int64/uint64/double | §6.6：未知数值**只允许 `raw`、不做类型转换**（故无越界问题）；已知数值按字段类型解析、越界拒绝；新增 `kJsonNumberOutOfRange`，并给出 5 项必测用例 | 已覆盖 |

### 12.2 当前状态

- **阶段 A：已审阅。** 范围、字段清单、旋转 Region 判据与 14 个 fixture 场景获认可。
- **本 RFC：v1.2 复审通过**（§11.1 B1 已关闭）。
- **阶段 B：已获准启动。** 先按 §7.4 完成依赖 vendor 与许可证登记，再开始 Codec 实现。
