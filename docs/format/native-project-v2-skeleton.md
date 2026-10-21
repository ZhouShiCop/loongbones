# 原生格式 v2（Skeleton 增量）Schema 冻结稿

- 文档状态：**B0 冻结稿（退回补项后修订，复核通过）**
- 日期：2026-10-20（初版）；2026-10-20（退回补项修订：`sourceRef` 改为整个对象可选）
- 上位文档：`docs/format/native-project-v1-rfc.md`（已审阅 v1 基线，**本文不改写它**）、
  `docs/skeleton/skeleton-domain-rfc.md`（D24–D28）、`docs/skeleton/canonical-visual-data-types.md`、
  `Agent_Task/Task4_Implementation_Plan.md` B0
- 定位：v2 = v1 + `project.skeletons[]`。**只序列化已冻结的首批类型**；
  Mesh/WeightedMesh、约束、动画不进入 v2，后续通过显式 capability / 版本演进。

### 证据范围声明（重要）

本文的字段设计依据**全部来自 Spine 3.2.01 语料**（1113 个 `.skel.bytes`）。
**尚未完成 DragonBones 数据对照**，因此本文的结论只代表「Spine 3.2.01 → Loongbones」这一条路径，
**不得**表述为跨格式通用结论。DragonBones 的格式研究、证据采集与导入器须**独立立项**；
在完成对照之前，不得据本文推断 DragonBones 也需要/不需要同样的字段与分层。

---

## 1. v2 相对 v1 的增量清单

| # | 增量 | 类型 |
|---|---|---|
| 1 | `project.skeletons[]` | 新数组（可空数组，但键必须存在） |
| 2 | `Skeleton` / `Bone` / `Slot` / `Skin` / `SkinEntry` / `RegionAttachment` / `SourceRef` | 新对象 |
| 3 | `OwnerKind` 新增 `kSkeleton` / `kBone` / `kSlot` / `kSkin` / `kSkinEntry` | 未知字段宿主种类 |
| 4 | `capabilities.required` 新增 `loongbones.skeleton.v1` | 必需能力 |
| 5 | `TextureAsset.storage = "inProject"` 的**位置格式**与校验规则（§7） | 资源落地策略 |

**v2 不改动**：v1 全部字段的含义、`formatId`、节点/场景/资产/图集 schema、
Reader 安全上限（D20）、未知字段保留机制（D18/D22）。

---

## 2. 完整 JSON 示例

下例是一个**可独立实现**的最小 v2 文档：一个工程、一个场景、一张图集（1 页 1 region）、
一个骨架（2 骨 / 1 槽 / 1 皮肤 / 1 个 RegionAttachment）。

> 本示例的骨架是**导入产生**的，因此带 `sourceRef`（§3.7）。
> 原生创建的骨架应**整个省略** `sourceRef` 键；除这一点外，其余字段与必需性完全相同。

```json
{
  "formatId": "loongbones.native-project",
  "formatVersion": 2,
  "minimumReaderVersion": 2,
  "capabilities": {
    "required": ["loongbones.atlas.v1", "loongbones.skeleton.v1"],
    "optional": []
  },
  "project": {
    "id": "01J8ZQ9XK2M8V0000000000001",
    "name": "spine32-first-slice",
    "rootNodeId": "01J8ZQ9XK2M8V0000000000002",
    "defaultSceneIndex": 0,
    "scenes": [
      { "id": "01J8ZQ9XK2M8V0000000000003", "name": "Scene", "rootNodeId": "01J8ZQ9XK2M8V0000000000002" }
    ],
    "nodes": [
      {
        "id": "01J8ZQ9XK2M8V0000000000002",
        "name": "root",
        "parent": null,
        "children": [],
        "type": "root",
        "transform": { "x": 0, "y": 0, "rotationDeg": 0, "scaleX": 1, "scaleY": 1 },
        "visible": true,
        "locked": false
      }
    ],
    "nodeOrder": ["01J8ZQ9XK2M8V0000000000002"],
    "assets": [],
    "textures": [
      {
        "id": "01J8ZQ9XK2M8V0000000000004",
        "name": "b0001s3.png",
        "storage": "inProject",
        "location": "assets/textures/01J8ZQ9XK2M8V0000000000004.png",
        "pixelWidth": 918,
        "pixelHeight": 918
      }
    ],
    "atlases": [
      {
        "id": "01J8ZQ9XK2M8V0000000000005",
        "name": "b0001s3",
        "pages": [
          {
            "id": "01J8ZQ9XK2M8V0000000000006",
            "name": "b0001s3.png",
            "textureAssetId": "01J8ZQ9XK2M8V0000000000004",
            "width": 918,
            "height": 918
          }
        ],
        "regions": [
          {
            "id": "01J8ZQ9XK2M8V0000000000007",
            "name": "region-0",
            "pageId": "01J8ZQ9XK2M8V0000000000006",
            "packedX": 0,
            "packedY": 0,
            "packedW": 32,
            "packedH": 32,
            "rotation": "none",
            "originalW": 32,
            "originalH": 32,
            "trimmedW": 32,
            "trimmedH": 32,
            "offsetX": 0,
            "offsetY": 0
          }
        ]
      }
    ],
    "skeletons": [
      {
        "id": "01J8ZQ9XK2M8V0000000000008",
        "name": "b0001s3",
        "atlasIds": ["01J8ZQ9XK2M8V0000000000005"],
        "boneOrder": [
          "01J8ZQ9XK2M8V0000000000009",
          "01J8ZQ9XK2M8V000000000000A"
        ],
        "bones": [
          {
            "id": "01J8ZQ9XK2M8V0000000000009",
            "name": "root",
            "parentId": null,
            "x": 0, "y": 0,
            "rotationDeg": 0,
            "scaleX": 1, "scaleY": 1,
            "shearXDeg": 0, "shearYDeg": 0,
            "inheritRotation": true, "inheritScale": true,
            "length": 0
          },
          {
            "id": "01J8ZQ9XK2M8V000000000000A",
            "name": "bone-1",
            "parentId": "01J8ZQ9XK2M8V0000000000009",
            "x": 4, "y": -2,
            "rotationDeg": 15,
            "scaleX": 1, "scaleY": 1,
            "shearXDeg": 0, "shearYDeg": 0,
            "inheritRotation": true, "inheritScale": true,
            "length": 12
          }
        ],
        "drawOrder": ["01J8ZQ9XK2M8V000000000000B"],
        "slots": [
          {
            "id": "01J8ZQ9XK2M8V000000000000B",
            "name": "slot-0",
            "boneId": "01J8ZQ9XK2M8V0000000000009",
            "color": { "r": 1, "g": 1, "b": 1, "a": 1 },
            "attachmentName": "region-0",
            "blendMode": "normal"
          }
        ],
        "skins": [
          {
            "id": "01J8ZQ9XK2M8V000000000000C",
            "name": "default",
            "entries": [
              {
                "id": "01J8ZQ9XK2M8V000000000000D",
                "slotId": "01J8ZQ9XK2M8V000000000000B",
                "attachmentName": "region-0",
                "attachment": {
                  "type": "region",
                  "regionId": "01J8ZQ9XK2M8V0000000000007",
                  "x": 0, "y": 0,
                  "rotationDeg": 0,
                  "scaleX": 1, "scaleY": 1,
                  "width": 32, "height": 32,
                  "color": { "r": 1, "g": 1, "b": 1, "a": 1 }
                }
              }
            ]
          }
        ],
        "sourceRef": {
          "formatId": "spine.binary",
          "formatVersion": "3.2.01",
          "sourceHeaderHash": "0f1e2d3c4b5a6978",
          "contentHash": "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08",
          "sourceUnit": "b0001s3",
          "importedBy": {
            "pluginId": "loongbones.importer.spine32",
            "pluginVersion": 1
          }
        }
      }
    ]
  }
}
```

> 示例中的 ID 为演示用的合法 ULID 形态；真实工程由宿主随机生成，**不得**由名称、下标或文件 hash 推导（D25）。

---

## 3. 字段表：必需性与默认值

约定：**必需** = 缺失即 `kFormatMissingField`；**可选** = 缺失取默认值；
「未知值」= 值不在枚举/范围内 → `kJsonTypeMismatch`。
所有 number 若出现 NaN/Inf 字面量，由 JSON 层先行拒绝（`kJsonNonFiniteNumber`，v1 §4.1）。

### 3.1 `project.skeletons[]` → `Skeleton`

| 字段 | 类型 | 必需性 | 默认 | 说明 |
|---|---|---|---|---|
| `id` | string（ULID） | 必需 | — | `IdKind::Skeleton` |
| `name` | string | 必需 | — | 展示名，工程内不要求唯一 |
| `atlasIds` | string[]（ULID） | 可选 | `[]` | 引用的图集；用于附件 → region 解析 |
| `boneOrder` | string[]（BoneId） | 必需 | — | 骨骼 ID 规范顺序；须与 `bones[].id` 互为排列 |
| `bones` | Bone[] | 必需 | — | 至少 1 个根骨；允许多个根骨 |
| `drawOrder` | string[]（SlotId） | 必需 | — | 绘制顺序（后者绘于上层）；须与 `slots[].id` 互为排列 |
| `slots` | Slot[] | 必需 | — | |
| `skins` | Skin[] | 必需 | — | 至少含 `default` |
| `sourceRef` | SourceRef | **可选** | 省略（无来源元数据） | §3.7。**原生创建的骨架省略它**；只有导入产生的骨架才带来源信息 |

**v2 不序列化** `constraints`（IK/transform 属后续任务）。将来新增为可选字段，不改变既有含义。

**根骨数量契约**：每个 Skeleton 必须至少有 1 个根骨，允许有多个根骨，不要求唯一。根骨定义为 `parentId` 省略的 Bone；所有非根骨的父引用必须有效，并满足本 Schema 的骨骼顺序与无环约束。该规则与 B2 Domain 校验一致。

### 3.2 `Bone`

| 字段 | 类型 | 必需性 | 默认 |
|---|---|---|---|
| `id` | string（ULID） | 必需 | — |
| `name` | string | 必需 | — |
| `parentId` | string?（BoneId） | 可选 | `null`（根） |
| `x` / `y` | number | 可选 | `0` |
| `rotationDeg` | number | 可选 | `0` |
| `scaleX` / `scaleY` | number | 可选 | `1` |
| `shearXDeg` / `shearYDeg` | number | 可选 | `0` |
| `inheritRotation` / `inheritScale` | bool | 可选 | `true` |
| `length` | number | 可选 | `0` |

单位与符号：Y-up、度、世界单位（ADR-0003 C1/C2）；`parentId` 必须指向**已声明**的骨骼，
禁止前向引用与成环（Domain 校验）。

### 3.3 `Slot`

| 字段 | 类型 | 必需性 | 默认 |
|---|---|---|---|
| `id` | string（ULID） | 必需 | — |
| `name` | string | 必需 | — |
| `boneId` | string（BoneId） | 必需 | — |
| `color` | ColorF | 可选 | `{r:1,g:1,b:1,a:1}` |
| `attachmentName` | string? | 可选 | `null` |
| `blendMode` | 枚举 | 可选 | `"normal"` |

`ColorF` = `{ "r": number, "g": number, "b": number, "a": number }`，四者均 **0..1**（ADR-0003 C8）。
越界（<0 或 >1）→ `kJsonTypeMismatch`。源 8-bit 颜色由导入器 `/255` 转换后写入。

`blendMode` 枚举字符串：`"normal"` / `"additive"` / `"multiply"` / `"screen"`。

### 3.4 `Skin`

| 字段 | 类型 | 必需性 | 默认 |
|---|---|---|---|
| `id` | string（ULID） | 必需 | — |
| `name` | string | 必需 | — |
| `entries` | SkinEntry[] | 必需 | `[]` |

### 3.5 `SkinEntry`（B0 冻结：**有稳定 ID**）

| 字段 | 类型 | 必需性 | 默认 |
|---|---|---|---|
| `id` | string（ULID） | 必需 | — |
| `slotId` | string（SlotId） | 必需 | — |
| `attachmentName` | string | 必需 | — |
| `attachment` | Attachment | 必需 | — |

**同一 `Skin` 内 `(slotId, attachmentName)` 必须唯一**（Domain 校验）；
`id` 的存在使未知字段宿主与数组重排解耦（§5）。

### 3.6 `Attachment`（v2 仅 `region` 一种）

| 字段 | 类型 | 必需性 | 默认 |
|---|---|---|---|
| `type` | 枚举 | 必需 | —，仅接受 `"region"` |
| `regionId` | string（AtlasRegionId） | 必需 | — |
| `x` / `y` | number | 可选 | `0` |
| `rotationDeg` | number | 可选 | `0` |
| `scaleX` / `scaleY` | number | 可选 | `1` |
| `width` / `height` | number | 可选 | `0` |
| `color` | ColorF | 可选 | `{r:1,g:1,b:1,a:1}` |

- `type` 取值仅为 `"region"`。任何其它值 → `kJsonTypeMismatch`；
  若未来版本新增 `"mesh"`，它将伴随独立 required capability（如 `loongbones.skeleton.mesh.v1`），
  能力检查先于类型检查，故旧 Reader 报 `kCapabilityUnsupported` 而非类型错误。
- `regionId` 必须指向**已存在**的 `AtlasRegion`（Domain 校验，跨集合引用闭合）。

### 3.7 `SourceRef`（整个对象**可选**）

**为什么可选**：Loongbones 的原生数据也必须能创建**没有任何外部来源**的骨架
（用户在编辑器里从零搭建的骨架、或由其它非导入路径产生的骨架）。
强制 `sourceRef` 会让原生创建的数据无处可填，或诱使实现者伪造来源信息。

| 情形 | 行为 |
|---|---|
| `sourceRef` **省略** | 合法。表示"原生创建，无来源信息"。不产生任何诊断 |
| `sourceRef` **存在** | 按上表逐字段校验；缺必需字段 → `kFormatMissingField`，类型/形态不符 → `kJsonTypeMismatch` |
| `sourceRef` 为 `null` | **不合法** → `kJsonTypeMismatch`。要么省略整个键，要么给完整对象；不用 `null` 表示"无来源" |

> `null` 与"省略"不等价：省略是"该字段未使用"，`null` 是"该字段有值且值为空"。
> 对可选对象统一用「省略」表达缺失，避免读者把 `null` 当作第三种状态处理。

| 字段 | 类型 | 必需性 | 默认 | 说明 |
|---|---|---|---|---|
| `formatId` | string | 必需 | — | 源格式标识，如 `spine.binary` |
| `formatVersion` | string | 必需 | — | 源格式版本，如 `3.2.01` |
| `sourceHeaderHash` | string | 可选 | 省略 | **源自带**头字段原文；无算法、不截断 |
| `contentHash` | string | 必需 | — | **宿主/插件对 `.skel.bytes` 全文**计算的 SHA-256（小写十六进制，64 字符） |
| `sourceUnit` | string | 可选 | 省略 | 源内逻辑单位（相对引用名），不含绝对路径/盘符/用户名 |
| `importedBy.pluginId` | string | 必需 | — | 导入插件标识 |
| `importedBy.pluginVersion` | integer | 必需 | — | 插件版本（仅来源记录与诊断，D28） |

`sourceHeaderHash` 与 `contentHash` 是**两个字段**：来源、算法、覆盖范围都不同
（Task 4 Back Task §5.1）。禁止把前者"原样"写进后者。纹理摘要**不进入工程**。

`sourceRef` 中的字段必需性只在**对象存在时**生效；对象整体省略时不需要任何占位值。

---

## 4. ID 规则

| 项 | 规则 |
|---|---|
| 形式 | ULID：26 字符 Crockford Base32，大写 |
| 生成 | 宿主 `IIdFactory` 随机生成；导入对象同样随机（D25） |
| 读取 | 容忍小写；写出统一大写 |
| 禁止 | 由名称、数组下标、文件路径、文件 hash 推导 |
| 格式校验 | Format 层只校验「是 string」；`isWellFormed()` / 种类 / 唯一性由 Domain `adoptData()` 负责 |
| 种类 | `IdKind::Skeleton` / `Bone` / `Slot` / `Skin` / `SkinEntry`（B2 新增）；Attachment 无 ID |

---

## 5. 未知字段宿主锚点（B0 冻结）

沿用 v1 §6.5.1：锚点 = `(OwnerKind, 稳定 ID)`，加宿主内**相对子路径**；
**不得**用数组下标作锚点。

### 5.1 新增 `OwnerKind`

| 新增值 | 宿主对象 |
|---|---|
| `kSkeleton` | `Skeleton` |
| `kBone` | `Bone` |
| `kSlot` | `Slot` |
| `kSkin` | `Skin` |
| `kSkinEntry` | `SkinEntry` |

`OwnerKind` 是**只追加**枚举；新增值不改变既有取值含义。

### 5.2 无 ID 的嵌套容器

| 容器 | 锚点 |
|---|---|
| `Skeleton.sourceRef` | `(kSkeleton, skeletonId)` + `/sourceRef` |
| `Skeleton.sourceRef.importedBy` | `(kSkeleton, skeletonId)` + `/sourceRef/importedBy` |
| `Slot.color` | `(kSlot, slotId)` + `/color` |
| `attachment`（RegionAttachment） | `(kSkinEntry, entryId)` + `/attachment` |
| `attachment.color` | `(kSkinEntry, entryId)` + `/attachment/color` |

> 这些容器都是**可选**的（`sourceRef` 整个对象可选，§3.7）。锚点规则只在容器**出现**时生效；
> 容器省略时不存在对应未知字段，也不需要占位。原生创建的骨架（无 `sourceRef`）因此不影响锚点集合。

### 5.3 为什么 `SkinEntry` 必须有 ID（B0 决策记录）

SkinEntry 是**可重排数组**。若沿用 v1「无 ID 容器用最近的有 ID 祖先 + 相对子路径」，
其锚点会变成 `(kSkin, skinId) + /entries/<下标>` —— 正是 D22 明令禁止的形态：
用户重排 entries 后，未知字段会挂到错误的条目上。

两种候选：

| 方案 | 结论 |
|---|---|
| **A. SkinEntry 增加 `id`（ULID）+ `OwnerKind::kSkinEntry`** | **采纳**。锚点变为 `(kSkinEntry, entryId)`，与位置完全解耦；代价是每个条目多一个 26 字符字段 |
| B. 用自然键 `(skinId, slotId, attachmentName)` 作锚点 | 否决。自然键**可被用户编辑**（重命名附件即失效），违反「锚点必须稳定」 |
| C. 证明无 ID 时重排后仍正确归属 | 无法证明：附件名与槽引用都可编辑，任何自然键都会漂移 |

**Attachment 不单独给 ID**：它与 SkinEntry 是 1:1 内嵌关系，
锚点 `(kSkinEntry, entryId) + /attachment` 已稳定；再给一个 ID 会制造"两个 ID 描述同一对象"的所有权歧义。
若将来 Attachment 需要被多个条目共享（Spine 的 linked mesh 不共享附件定义，只共享网格），
届时作为独立数组引入并新增 `OwnerKind`，属演进而非修订。

---

## 6. 能力与版本

| 项 | v2 规则 |
|---|---|
| `formatId` | `loongbones.native-project`（不变） |
| `formatVersion` | `2` |
| `minimumReaderVersion` | `2`（写出时不低于 2） |
| `capabilities.required` | 必须含 `loongbones.atlas.v1`（v1 基线）+ **`loongbones.skeleton.v1`** |
| `capabilities.optional` | 不认识的条目忽略但保留（v1 §1.2） |

**`loongbones.skeleton.v1` 的适用范围（已裁定）**：v2 中 `project.skeletons` 键**恒存在**
（可为空数组），因此「有 `skeletons[]` 的 v2 文档」= 全部 v2 文档 → **每个 v2 文档都声明该能力为 required**。
不这样做的替代读法是"仅当 skeletons 非空时才要求"，但那会让 Reader 必须先解析数据才能判断能力是否匹配，
与「打开时即明确表态」的意图相反，故不采用。

> **负责人 2026-10-20 裁定**：同意每个 v2 文档都声明为 required；`project.skeletons` 是 v2 必需键，
> 即使为空，也可以在读取数据之前检查能力声明。

### 6.1 读取顺序与错误码（固定，不二选一）

1. `formatId` 不匹配 → `kFormatIdUnsupported`
2. `formatVersion` 超出本 Reader 范围（含旧 v1 Reader 读到 2）→ **固定 `kFormatVersionUnsupported`**
3. `minimumReaderVersion` 高于本 Reader 上限 → `kFormatTooNew`
4. `capabilities.required` 含未知项 → `kCapabilityUnsupported`
5. 之后才进入 JSON 结构 / 字段 / Domain 校验

四类版本/能力错误**各自单一、互不替代**（D24 追加裁定）。

---

## 7. 资源落地策略（B0 冻结）

### 7.1 首片策略：复制进工程

| 项 | 规则 |
|---|---|
| 导入产出的 `TextureAsset.storage` | 恒为 `"inProject"` |
| `location` 格式 | `assets/textures/<TextureAssetId>.png`（工程相对路径，POSIX `/` 分隔符） |
| 命名依据 | 用 **TextureAsset ID** 而非 `name`：`name` 可重命名/可重复，ID 不可变且唯一 |
| 扩展名 | 与源 PNG 一致（`.png`）；源为其它格式时用该格式扩展名 |
| `kExternalReference` | 保留给用户显式创建的外部引用；**导入器永不产出** |

### 7.2 读写路径与权限

```
源 PNG（只读，授权根目录内）
  → IResourceResolver 只读解析（按图集页头 file，不按 skeleton 文件名猜）
  → 宿主资源暂存服务读入内存
  → [Domain 候选校验通过]
  → 写入工程内 assets/textures/<id>.png（先临时文件，校验 hash/尺寸，再原子替换）
  → saveProject() 原子替换 project.json
```

- **插件不获得任意路径写权限**；只有宿主的资源暂存服务写盘。
- 资源文件在 Domain 校验通过**之后**才落盘；失败则回滚新增文件，旧工程保持有效。
- 进程崩溃最多遗留**未被引用**的文件，不得出现 `project.json` 指向缺失文件的窗口。

### 7.3 Format 层对 `location` 的形状校验（`storage == "inProject"`）

| 规则 | 违反时错误码 |
|---|---|
| 必须是**相对**路径（不得以 `/`、盘符 `X:` 或 `\\` 开头） | `kJsonTypeMismatch` |
| 不得含 `..` 段 | `kJsonTypeMismatch` |
| 分隔符统一为 `/` | `kJsonTypeMismatch` |
| 不得为绝对路径 / 含用户名 | `kJsonTypeMismatch` |

Format 层**不校验文件是否存在**（存在性由宿主在打开/保存时处理，与 v1 对 `location` 的立场一致）。
`storage == "external"` 时不做上述形状限制（用户显式选择外部引用，路径语义由后续资产模块定义）。

---

## 8. 迁移契约（v1 → v2）

`migrate1to2(JsonValue) -> JsonValue`，**纯函数**：

| 步骤 | 动作 |
|---|---|
| 1 | `formatVersion`：`1` → `2` |
| 2 | `minimumReaderVersion`：置为 `2` |
| 3 | `capabilities.required`：追加 `loongbones.skeleton.v1`（若 `capabilities` 缺失则创建，`required` 只含该能力 + 基线能力） |
| 4 | `project.skeletons`：缺失则补 `[]`；已存在则原样保留 |
| 5 | 其余字段、未知字段及其锚点：**逐一原样保留** |
| 6 | 不生成任何 ID、不改既有字段值、不读时钟、不碰磁盘 |

**幂等**：对已是 v2 的输入恒等（步骤 1/2/3 为幂等设置，步骤 4 不覆盖已有值）。

**保存时机**：迁移结果先进入内存（`ProjectDocument`）；**只有用户显式保存时才写 v2**。
未保存退出 → 磁盘工程保持 v1 字节不变。

---

## 9. 诊断等级（B0 冻结，Task 4 阶段 B 验收范围）

| 等级 | 触发 | 后果 |
|---|---|---|
| **error** | 结构损坏、非法引用（父索引越界/前向引用/成环、slot→bone、skin→slot、entry/attachment 唯一性、drawOrder 非排列、region 不可解析）、缺纹理、非有限浮点、未知附件类型、不支持的源版本 | **拒绝整个候选**，不提交任何对象 |
| **warning** | 源里存在本任务不支持的能力（动画时间线、mesh/约束参数、事件、path 约束、boundingbox、sequence index、多皮肤换装）；附件名尾随空格的降级匹配 | 候选**可提交**，但**仅在用户明确确认后**；未进入切片的数据只产生统计与诊断，**不创建半成品 Domain 对象** |

排序：先 Format 后 Domain；Domain 内按 document order（先出现先报）。

---

## 10. v2 明确不包含的内容

| 不包含 | 去向 |
|---|---|
| `Skeleton.constraints`（IK / transform / path） | 后续任务 + 独立 capability |
| `MeshAttachment` / `WeightedMesh` / linked mesh | 后续任务 + 独立 capability |
| 动画（全部时间线）、deform、事件 | 后续任务 |
| 场景中的骨架实例节点（`SkeletonNode`） | 会改 v1 `SceneNode::type` 枚举，另立任务（D27） |
| 多皮肤换装、皮肤混合 | 后续任务（语料无真实样本，T-B4） |
| 外部引用纹理的导入产出 | 保留 `external` 枚举值供用户显式使用，导入器不产出 |

---

## 11. 与既有文档的对应

| 问题 | 答案所在 |
|---|---|
| 为什么需要独立 Skeleton Domain | `docs/skeleton/skeleton-domain-rfc.md` §2/§3 |
| 类型分层与几何/UV 语义 | `docs/skeleton/canonical-visual-data-types.md` |
| 源字段 → canonical 逐字段映射 | `docs/skeleton/field-mapping.md` |
| 插件边界与宿主接口 | `docs/skeleton/plugin-host-rfc.md` |
| 已知 v1 顶层/对象键集合 | `format/src/decode.cpp`（`kTopKeys` / `kProjectKeys` / …） |
| 既有错误码全集 | `format/include/loongbones/format/format_diagnostics.hpp` |
