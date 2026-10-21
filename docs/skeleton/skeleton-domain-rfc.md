# Skeleton Domain 与原生格式兼容提案（返修稿）

- 文档状态：**阶段 A 复核通过；B0 契约冻结复核通过**；B1–B6 产品代码工作包**未获批准、未启动**。
- 返修日期：2026-10-20（初稿 2026-10-20；复核发现项见 `ACCEPTANCE.md` §7）
- 任务来源：`Agent_Task/Task4.md` §2.2 + `Agent_Task/Task4_Back_task.md`
- 依据：`docs/format/native-project-v1-rfc.md`（已审阅基线，**本文件不修改它**）、
  `docs/mapping-spine32-to-native.md`、`docs/adr/0003-units-and-coordinate-contract.md`、
  `docs/adr/0004-stable-identity-strategy.md`（**已定案**）、`DECISIONS.md` D1–D28、
  `项目规范.md` §3/§5/§8/§9/§12
- 证据来源：`tools/skeleton-probe/`（只读探针，语料口径见 §12）

### 证据范围声明

本文件的全部语料证据来自 **Spine 3.2.01**（1113 个 `.skel.bytes`）。
**尚未完成 DragonBones 数据对照**：文中所有"语料实测"数字与结论仅覆盖 Spine 这一条路径，
**不得**表述为跨格式通用结论。DragonBones 的格式研究、证据采集与导入器须**独立立项**。

### 已裁定事项在本文件中的落点

| 裁定 | 落点 | 决策号 |
|---|---|---|
| 原生格式采用 **v2** 承载 Skeleton | §4 | D24 |
| 持久化对象一律随机 ULID，源 hash 不入 ID 派生 | §7 | D25 |
| 插件编译期静态注册 | 见 `plugin-host-rfc.md` §3 | D26 |
| Skeleton 只作 Project 级资产，不入 Scene | §3.2 | D27 |
| 不提供插件版本迁移钩子 | §9.4 | D28 |

---

## 1. 范围与术语

### 1.1 范围

本文件交付**设计契约**。**必须区分两件事**（Back Task §2.1）：

| 概念 | 含义 | 在哪里声明 |
|---|---|---|
| **骨架模型提案（model proposal）** | Skeleton/Bone/Slot/Skin/Attachment/Constraint 的完整字段设计 | §5（**提案**，非已发布 schema） |
| **Task 4 垂直切片范围（vertical slice）** | 本任务阶段 B **实际要实现**的子集 | §5.6、§10.1 |

两者不等价：**提案里出现的字段 ≠ 本任务会实现**。凡 §5 中未列入 §10.1 的能力
（mesh/加权 mesh/linked mesh、IK/transform 参数、动画/deform 等），一律标注为
「后续任务或未来模型提案」，**不得**称为本任务的"首批支持"或"已支持"。

| 交付 | 位置 |
|---|---|
| Skeleton Domain 与既有 Domain 的职责边界 | §3 |
| 与原生格式 v1 的兼容方式（已裁定 v2 + 迁移契约） | §4 |
| 骨架数据模型提案 | §5 |
| 语义映射（坐标系、变换、继承、绘制顺序、缩放、trim/rotation、UV、颜色、非有限浮点） | §6 |
| ID 与稳定键 | §7 |
| 未知/不支持/诊断契约 | §8、§9 |
| 支持矩阵（实现范围 / 提案 / 拒绝 / 待验证） | §10 |
| 样本 manifest、合成 fixture、探针 | `sample-manifest.md`、`fixtures/README.md`、`../../tools/skeleton-probe/README.md` |

不交付：产品读取器实现、GUI、渲染（属阶段 B/后续任务；本文件**不宣称**已有 v2 Reader/Writer）。

### 1.2 术语

| 术语 | 含义 |
|---|---|
| **Skeleton（骨架）** | 可复用的骨骼层级 + 插槽绘制顺序 + 附件定义的**资产级对象**。不是场景节点树的一部分 |
| **Bone（骨骼）** | 骨架内部的层级单元，用于驱动附件的局部变换。**不是 Scene Node**（§3.2） |
| **Slot（插槽）** | 骨架内部的绘制槽位，持有一个当前附件、颜色与绘制顺序 |
| **Attachment（附件）** | 挂在 (Skin, Slot) 上的可绘制内容（region/mesh 等） |
| **Skin（皮肤）** | (Slot, 附件名) → 附件 的映射表。**归属 Skeleton**（§3.1），不是 Project 顶层集合 |
| **Atlas / AtlasRegion / AtlasPage** | 沿用 `native-project-v1-rfc.md` 的既有定义 |
| **源索引（source index）** | 源格式里出现的**数组下标**（骨骼/插槽/约束引用）。**不得**作为持久化 ID（§7） |
| **稳定键（stable key）** | 从源数据**语义路径**派生的键（如 `bone:chest`）。**仅用于单次导入内**把源引用解析到宿主分配的 ID；不参与 ID 派生、不用于去重复用（D25） |
| **垂直切片（vertical slice）** | 本次导入可端到端打通的最小能力集合（§10.1） |

---

## 2. 为什么需要独立的 Skeleton Domain

Spine 骨架是一套「层级 + 绘制顺序 + 附件解析」的**正交结构**：它的层级与
`SceneNode` 层级**语义不同**（骨骼不是可视节点、不参与场景渲染树），绘制顺序由
`slot` 数组顺序而非节点顺序决定，附件解析依赖「骨架 ↔ 图集」的跨资产查找。

若把骨骼降级成 Scene Node，会立刻撞上三个已定案约束的冲突：
- `native-project-v1-rfc.md` 的 `nodeOrder` 冗余校验语义对骨骼层级不成立；
- `SpriteNode` 必须引用 `AtlasRegion`，而骨骼本身**没有**区域；
- `DECISIONS.md` D3/ADR-0003 的 Y-up 与单位契约虽可直接沿用，但「父变换继承 + 绘制顺序」是骨骼自有的。

因此：**Skeleton 是与 Atlas 同级的资产对象**，由场景中的**独立实例类型**引用（§3.2），
而不是节点树的一部分。

---

## 3. 职责边界

### 3.1 与 Project / Asset / Atlas

```
ProjectData
├── version / capabilities / meta
├── assets[] / atlases[] / scenes[] / nodes[]
└── skeletons[]        ← 新增（资产级，与 atlases 并列）
     └── Skeleton.skins[]   ← Skin 归属 Skeleton
```

- `skeletons[]` 由 **Project 拥有**（与 `atlases[]` 同级），不是 Scene 的私有数据。
  理由：骨架是可复用资产，可被多次实例化，也可被多个场景共用。
- **Skin 只有一种归属：`Skeleton.skins[]`**。Project 顶层**没有**也无计划新增与
  Skeleton 并列的 `skins[]`（v1 顶层无此集合；Spine 的 Skin 语义上从属于骨架）。
- `Skeleton` 引用 `Atlas` / `AtlasRegion` / `TextureAsset` 的方式与既有资产引用一致
  （ID 引用 + 引用完整性校验）。
- **不新增** `Atlas` / `AtlasRegion` / `TextureAsset` 的字段（§6.7 的缩放结论表明现有字段已够用）。

### 3.2 与 Scene / SceneNode（关键边界，已定案 D27）

| 维度 | Scene Node | Skeleton Bone |
|---|---|---|
| 是否在场景渲染树中 | 是 | **否**（骨架内部层级） |
| 是否有可视内容 | 有（SpriteNode 引用 AtlasRegion） | 无（骨骼只是变换载体，可视内容在 Slot/Attachment） |
| 绘制顺序 | `parent.childOrder` | **slot 数组顺序**（骨架内部 `drawOrder`） |
| 是否被节点树冗余校验约束 | 是（parent.childOrder ↔ node.parentId） | 否 |
| 是否可被实例化多次 | 否（节点唯一） | 是（一个骨架可被实例化多次） |

**决定（D27）**：

- Task 4 导入的 Skeleton 是 **Project 级资产**；导入**不创建 Scene 实例**、不向 Scene
  写入任何内容。
- 未来在场景中放置骨架，必须由**专用的骨架实例类型**引用 Skeleton 资产 ID。
  该类型会向 `SceneNode::type` 枚举新增值，属**改变已发布 v1 语义**，
  须另行立项并在 v2（或后续版本）的 RFC 中定义；**不在本任务权限内**。
- **不得**把 Bone 映射为 Scene Node，也**不得**把骨架伪装成普通 Sprite/ImageNode
  （那会既丢失层级语义，又制造出"看起来是图层、实际是骨骼"的混合模型）。

### 3.3 与渲染/求值

骨架的运行时求值（世界变换、附件顶点、绘制顺序、IK/transform 约束求解）属于**渲染/求值模块**，
与本 Domain 的解耦方式：Domain 只存**静态可编辑数据**（局部变换、层级、附件参数、约束参数），
不缓存任何求值结果。理由：`DECISIONS.md` D5（运行时 IR 严格分离）与 D11（渲染不得反向污染资产层）。

---

## 4. 与原生格式 v1 的兼容方式（已裁定：v2）

### 4.1 裁定结论（D24）

**采用原生格式 v2 承载 Skeleton。**

- v2 = v1 + 新增 `project.skeletons[]`（以及 §5 所需的少量新字段）。
- **v1 Reader 明确拒绝 v2**：`formatVersion = 2` 超出 v1 Reader 的 `kReaderMaxVersion`，
  **固定返回 `FORMAT_VERSION_UNSUPPORTED`**（不在多个版本类错误码之间二选一；
  `minimumReaderVersion` 超限走对应的 reader-version 错误码，见 §4.4）。
- **v1 → v2 是纯函数迁移**，**保留未知字段**，**先在内存中使用**；
  **只有用户保存时才写出 v2**。用户不保存，磁盘上的工程仍是 v1。
- **禁止**把骨架作为 v1 可选字段或未知字段透传（§4.2）。
- 含 `skeletons[]` 的 v2 文档还必须声明 `loongbones.skeleton.v1` 为 **required**
  能力；该能力**不替代**版本检查（§4.4）。

### 4.2 为什么不用 v1 可选字段 / 未知字段透传

（Back Task §2.1：删除"选项 A 仅因 required capability 才安全"的自相矛盾论证，
改为分别说明两类机制的真实风险。）

**(a) 可选未知字段透传**：`native-project-v1-rfc.md` 的未知字段机制保证的是
**round-trip 语义保留**（不丢字节），**不是语义安全**。旧 Reader 打开含骨架的 v1 工程后：

- 骨架对它是不透明未知字段，用户仍可编辑场景/资产并保存；
- 若用户删除了骨架引用的纹理或图集，旧 Reader 无法察觉，保存后工程内出现**悬挂引用**；
- 该坏工程再被新 Reader 打开时，已错过"打开时拒绝"的时机，只能靠运行时校验兜底 ——
  一致性问题从"打开失败"降级成"使用中异常"。

**(b) required capability 拒绝语义**：`capabilities.required` 里出现未知项时，
Reader **明确报错拒绝打开**（现有 codec 已实现为 `kCapabilityUnsupported`）。
它保证的是"旧 Reader 不会静默打开不认识的内容"，**不保证**新 Reader 能把骨架当可编辑数据
（那需要 Domain 层真正认识 `skeletons[]`）。

**结论**：(a) 会把一致性问题推迟到运行时；(b) 只是拒绝语义、不是承载机制。
两者都无法替代"新格式版本 + 显式迁移"：骨架是**核心可编辑数据**，必须让 Reader 在
**打开时**就明确表态"认识 / 不认识"，而不是"能打开但看不见"。

**(c) 未知字段塞源片段**：把源格式整段（或未映射字段清单）塞进未知字段，等于让 Spine 字节
成为隐藏主数据，**直接违反** `项目规范.md` §2.3.6。**一票否决**，不再作为备选。

### 4.3 历史备选（保留仅作记录，不再作为选项）

| 备选 | 做法 | 未被采用的原因 |
|---|---|---|
| 旧 A | 留在 v1，`skeletons[]` 作新增可选字段 | §4.2(a)：未知字段 round-trip 不保证语义安全，可产出悬挂引用 |
| 旧 B | 升到 v2 | **已采纳**（即本方案） |
| 旧 C | 留在 v1，源数据以未知字段/`sourceFragment` 保留 | §4.2(c)：违反规范 §2.3.6 |

### 4.4 迁移契约（供阶段 B 实现）

**迁移函数**：`migrate1to2(JsonValue) -> JsonValue`

| 要求 | 说明 |
|---|---|
| 纯函数 | 不读时钟、不生成 ID、不依赖外部状态、不改既有字段内容 |
| 只做"补默认值" | 追加空的 `project.skeletons: []`；v1 已有字段逐一原样保留 |
| 保留未知字段 | v1 的未知字段（含其宿主锚点）在 v2 中原样保留 |
| 不重写 ID | 迁移不新建、不改写任何对象 ID（D25） |
| 幂等 | `migrate1to2` 对已是 v2 的输入必须是恒等 |

**版本字段**

| 字段 | v1 | v2（迁移/写出后） |
|---|---|---|
| `formatId` | `loongbones.native-project` | 不变 |
| `formatVersion` | `1` | `2` |
| `minimumReaderVersion` | `1` | `2`（**不得**低于 2） |
| `capabilities.required` | 基线能力（现有 codec 为 `loongbones.atlas.v1`） | 基线能力 **+ `loongbones.skeleton.v1`**（见下） |

**能力标识（已裁定）**：含 `skeletons[]` 的 v2 文档**必须**把 `loongbones.skeleton.v1`
声明为 **required**；不认识该能力的 Reader **必须拒绝打开**（现有 codec 已实现为
`kCapabilityUnsupported`）。该能力**不替代** v2 版本检查 —— 两者各自独立生效：
版本不对报版本错，版本对但能力不认识报能力错。

**错误码（已裁定，各自单一固定）**：

| 情形 | 固定错误码 |
|---|---|
| 旧 v1 Reader 读到 `formatVersion = 2` | `FORMAT_VERSION_UNSUPPORTED`（**固定此码**，不在多个版本类错误码之间二选一） |
| `minimumReaderVersion` 高于当前 Reader 能读的上限 | 对应的 reader-version 错误码（现有 codec 为 `FORMAT_TOO_NEW`） |

两条规则互不替代：前者判"文档版本我认不认识"，后者判"文档要求的最低 Reader 我够不够"。

**保存时机**：迁移结果**先进入内存**（`ProjectDocument` / `ProjectData`），
**只有用户显式保存时才落盘 v2**。禁止"打开即改写磁盘上的工程"。
未保存就退出 → 磁盘工程保持 v1 字节不变。

**回归要求**：v1 全量 fixture → 迁移 → 再编码 → 与手写的 v2 期望 JSON 逐字段一致；
且 `migrate1to2` 的幂等性有用例。

### 4.5 与 Task 3 codec 现有能力的不一致（阶段 B **前置实现任务**）

以下均为**尚未实现**的能力缺口；在补齐前，不得宣称已有 v2 Reader/Writer。

| # | 现状（Task 3 codec，只读实测） | 需要的改动 |
|---|---|---|
| 1 | `codec.hpp`：`kFormatVersion = 1`、`kReaderMaxVersion = 1` | `kReaderMaxVersion` 提升到 2；新增 v2 读取路径 |
| 2 | `decode.cpp`：`formatVersion < 1 \|\| > kReaderMaxVersion` → `kFormatVersionUnsupported` | v1 文档仍可读；v2 文档走新路径；`minimumReaderVersion > 2` 走 `kFormatTooNew` |
| 3 | 无任何迁移函数 | 新增 `migrate1to2`（§4.4） |
| 4 | `encode.cpp`：`minimumReaderVersion` 写出 `kFormatVersion`（=1） | 写 v2 时必须写 2 |
| 5 | `codec.hpp`：`kBaselineCapability = "loongbones.atlas.v1"` | 新增 `loongbones.skeleton.v1`；含 `skeletons[]` 的 v2 文档必须把它列为 **required**（§4.4 已裁定，不留待阶段 B） |
| 6 | `ProjectData` 无 `skeletons[]` | 新增 Domain 集合 + 解码/编码 + 校验（§9） |
| 7 | 无"内存迁移、保存才落盘"的宿主层行为 | 属宿主/应用层，需在阶段 B 明确写入流程（打开→内存迁移→用户保存→写 v2） |

---

## 5. 数据模型提案

> 命名沿用 v1 既有风格（`snakeCase`、ID 为 ULID、颜色 `0..1` 浮点、布尔 `isXxx`）。
> **本节是提案，不是已发布 schema，也不等于本任务的实现范围**（§1.1、§5.6、§10.1）。

### 5.1 Skeleton

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` | `string`（ULID） | 宿主分配（§7.1） |
| `name` | `string` | 非唯一，可重复 |
| `atlasIds` | `string[]` | 引用的图集（ID），用于附件 → region 解析 |
| `boneOrder` | `string[]` | 骨骼 ID 的规范顺序（层级存在多种等价拓扑序，故需显式顺序） |
| `bones` | `Bone[]` | §5.2 |
| `drawOrder` | `string[]` | **Slot ID 的有序数组**，即绘制顺序（后排在上层） |
| `slots` | `Slot[]` | §5.3 |
| `skins` | `Skin[]` | §5.4，至少含 `default` |
| `constraints` | `Constraint[]` | §5.5（IK / transform） |
| `sourceRef` | `SourceRef?` | 源引用元数据（§8.3）。**整个对象可选**：导入时提供，原生创建时省略。**不含任何源字节** |

### 5.2 Bone

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` / `name` | | |
| `parentId` | `string?` | 空 = 根；**必须指向已声明的骨骼**（禁止前向引用，§6.2） |
| `x` / `y` | `number` | 相对父骨的局部平移，单位 = 世界单位（ADR-0003 C1） |
| `rotationDeg` / `scaleX` / `scaleY` | `number` | 局部旋转（度，ADR-0003 C2）/ 缩放 |
| `shearXDeg` / `shearYDeg` | `number` | 局部错切（度）。**3.2 有此字段**，语料 189,120 条 shear 时间线 → 必须建模 |
| `inheritRotation` / `inheritScale` | `bool` | 3.2 用两个独立布尔，**不是** 3.6+ 的 `transform` 枚举（§6.4） |
| `length` | `number` | 仅编辑器可视辅助，不影响求值 |

### 5.3 Slot

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` / `name` | | |
| `boneId` | `string` | 必填，必须指向本骨架内的骨骼 |
| `color` | `ColorF` | `0..1` 浮点 rgba（ADR-0003 C8） |
| `attachmentName` | `string?` | 默认皮肤下的当前附件名（可空 = 不显示） |
| `blendMode` | 枚举 | `normal / additive / multiply / screen` |

> 绘制顺序：slot **不自带** z 值，绘制顺序由 `Skeleton.drawOrder` 唯一确定；
> `drawOrder` 与 `slots[]` 必须互为排列（§9 冗余校验）。

### 5.4 Skin

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` / `name` | | `default` 为默认皮肤 |
| `entries` | `SkinEntry[]` | (Slot, 附件名) → 附件 |

**SkinEntry（B0 冻结：有稳定 ID）**

| 字段 | 类型 | 说明 |
|---|---|---|
| `id` | `string`（ULID） | 宿主分配。**必须存在**：entries 是可重排数组，无 ID 则未知字段会随重排错绑（v2 schema §5.3） |
| `slotId` | `string` | 指向本骨架内的 Slot |
| `attachmentName` | `string` | 与 `slotId` 组合在同一 Skin 内唯一 |
| `attachment` | Attachment | 内嵌值对象，**不单独给 ID**；未知字段锚定到 `(kSkinEntry, entryId) + /attachment` |

附件（**提案中的完整集合**；哪些属于 Task 4 范围见 §5.6）：

| 类型 | 字段 | Task 4 范围 |
|---|---|---|
| `RegionAttachment` | `type: "region"`、`regionId`（指向 `AtlasRegion`）、`x/y`、`rotationDeg`、`scaleX/scaleY`、`width/height`、`color` | **范围内** |
| `MeshAttachment` | `type: "mesh"`、`regionId`、`uvs`（regionUVs，`0..1`，ADR-0003 C5a）、`triangles`（顶点索引）、`vertices`、可选 `weights/bones`（加权绑定）、可选 `parentName`（linked mesh） | **范围外**（后续任务 + 独立 capability） |

`boundingbox` / `clipping` / `path` 附件：语料 0 例（§10），**不支持且有诊断**。

### 5.5 Constraint

| 类型 | 字段 | Task 4 范围 |
|---|---|---|
| `IkConstraint` | `name`、`boneIds[]`（有序链）、`targetBoneId`、`mix`（0..1）、`bendPositive`、`compress/stretch`（可选） | **范围外**（后续任务） |
| `TransformConstraint` | `name`、`boneId`、`targetBoneId`、`rotateMix/translateMix/scaleMix/shearMix`、偏移字段 | **范围外**（后续任务） |

`PathConstraint`：语料 **0 例**，不支持且有诊断。

### 5.6 Task 4 垂直切片范围（本任务阶段 B 要实现）

**范围内**：骨架元数据（源格式 id / 版本 / 尺寸 / hash 等 `sourceRef`）、`Bone`、
`Slot`、`Skin`（含 `default`）、`RegionAttachment`、以及
**Atlas / AtlasRegion / AtlasPage 的映射与校验**。

**范围外**（属后续任务或仅未来模型提案，**不得**标为本任务"首批支持/已支持"）：
mesh / 加权 mesh / linked mesh、IK / transform 约束参数、动画（全部时间线）、
deform、事件、多皮肤换装、皮肤混合。

---

## 6. 语义映射（逐项：证据来源 → 转换规则 → golden 值计划）

> 语料口径：1113 个 `.skel.bytes`（Full）/ 963（primary），全部 `3.2.01`，
> 解析成功 1113/1113、strictEof 失败 0。证据命令见 `tools/skeleton-probe/README.md`。

### 6.1 坐标系与单位

| 项 | 结论 | 证据 | golden 值计划 |
|---|---|---|---|
| Y 轴方向 | **Y-up，不做符号翻转** | ADR-0003 C1 已定案；Spine 同为 Y-up | fixture `02-nested`：hip.y=10 → 存 `y: 10` |
| 单位 | 1 = 1 世界单位 | ADR-0003 C1 | 同上 |
| 角度单位 | 度 | ADR-0003 C2 已定案；Spine 二进制存度 | fixture `02-nested`：chest.rotation=15 → `rotationDeg: 15` |
| 颜色 | `0..1` 浮点 rgba；源 8-bit → `/255` | ADR-0003 C8 | 源 `0xFFFFFFFF` → `{1,1,1,1}` |

### 6.2 父骨索引 → 引用

- 证据：语料 **骨骼父索引越界 0、前向引用 0、自环 0**；根骨骼 1113 个（每骨架 1 个根）。
- 规则：`parentIndex == -1` → `parentId = null`；否则必须 `< 自身下标`，且指向已声明骨骼。
- 校验：`parentId` 前向引用 / 越界 / 成环 → **拒绝整个候选**（§9）。
- golden：fixture `03-bad-parent-index` 的 `parentIndex = 9`（越界）→ 必须拒绝并给出可定位诊断。

### 6.3 局部变换与 shearing

- 源字段（3.2）：`x, y, rotation, scaleX, scaleY, shearX, shearY` + `length`。
- 规则：逐一映射（§5.2）。**shear 必须建模**：语料 189,120 条 shear 时间线（Full），
  丢失会导致动画形变错误。
- golden：fixture `02-nested` 的 `weapon` 附件 `rotation=90, x=4, y=-2` → `rotationDeg: 90, x: 4, y: -2`。

### 6.4 继承标志

- 证据：3.2 二进制用两个独立布尔 `inheritRotation` / `inheritScale`。
- 规则：原样映射为两个布尔。**不**伪造 3.6+ 的 `transform` 枚举（`normal/onlyTranslation/…`）——
  那是语义不同的概念，伪造会造成「看起来支持、实际不对」。
- 语料未统计布尔分布（非阻塞），列为 T-A2（§11.2）。

### 6.5 绘制顺序

- 证据：Spine 无 `zIndex`，slot 数组顺序即绘制顺序；语料 slot 名**零重名**（1113/1113）。
- 规则：`Skeleton.drawOrder` 为 slot ID 有序数组；`slots[]` 与 `drawOrder` 必须互为排列。
- 动画期的 `drawOrder` 时间线（语料 14,989 条 Full）**不支持且有诊断**（§10），
  但静态顺序必须正确保留。

### 6.6 slot 颜色与混合

- 源：`color`（8-bit RGBA）+ `blend`（0..3）。
- 规则：颜色 → `ColorF`（`/255`）；blend → 枚举。
- **预乘 alpha**：语料 1111/1111 页头**未声明** `pma`；按 libGDX 语义缺省为非预乘 → 无需反预乘。
  若将来遇到 `pma: true`，必须显式处理（T-B1）。
- 语料 `format` 全为 `RGBA8888`（1111/1111）。

### 6.7 图集缩放的承载层（关键结论）

- 现象：页头 `size` 普遍**小于**伴随 PNG 的 IHDR 尺寸（1107/1111 不一致，仅 4 例相等）。
- 判据（`png-page-probe.mjs`，只读 alpha，**不落地任何图像**）：
  - 非透明像素外接矩形 ≈ PNG 尺寸（而非页声明尺寸）→ 26/26 支持 H2；
  - 「落在页声明尺寸框内」的不透明像素占比 0.22–0.30，与 H2 预测值 0.25–0.27 吻合，
    与 H1 预测的 ≈1.0 相差数量级；
  - 结论分布：**H2 26 / H1 0 / inconclusive 3**，另有 1 例 `unsupported`。
- **结论**：region 坐标空间 = **页头声明的 `size`**；PNG 是更高分辨率的同一张图。
  缩放因子 = `pixelWidth / page.width`。
- **承载层 = 图集页层**：`AtlasPage.width/height` 保存图集坐标空间，`TextureAsset.pixelWidth/pixelHeight`
  保存实际像素；比值在渲染/求值时**于页层应用一次**。
- **v1 现有字段已足以承载，无需为此新增字段。**
- 残留待验证：**rotation ≠ 0 与页缩放的复合**（`DECISIONS.md` V2 的另一半）。语料 `rotate: true`
  占 **17734/43704 = 40.6%**，故这一项必须在 golden 阶段覆盖（T-A1）。

### 6.8 trim / rotation / sequence

| 源字段 | 结论 | 证据 |
|---|---|---|
| `offset: x,y` | → `AtlasRegion.offsetX/Y`；语料 **trim 0 例**（`size == orig` 100%） | 探针 §5 |
| `rotate: true/false` | → `AtlasRegion.rotation` 枚举 `none/90`；语料 true 17734 / false 25970 / 其它写法 **0** | 探针 §5 |
| `index` | **不落入 v1**（`AtlasRegion` 无此字段）；语料全为 `-1`（0 例非 -1） → 不支持且有诊断 | 探针 §5 |
| `split` / `pad` | v1 无对应；语料未出现 | 待验证 T-B2 |

### 6.9 附件 `path` 与 region 查找

- 规则：查找键 = **`path ?? name`**（Spine 规则）。
- 证据：按 `path ?? name` 命中 **47705 / 47709**；仅按附件名命中 47632（少 77）。
  即语料中有 **84 个附件显式写了 `path`**，且**全部**与附件名不同（84/84）。
- **未命中 4 例（4 个不同单位，各 1 个附件）**：根因是**附件名带尾随空格**
  （实测为 `"head_back "`、`"mouth_open1 "` 之类，图集里是 `head_back`、`mouth_open1`）。
- 契约：
  1. 精确匹配（`path ?? name`）；
  2. 未命中 → **必须产出诊断** `SKELETON_ATTACHMENT_REGION_UNRESOLVED`（带 slot/附件位置）；
  3. 可选按 `trim()` 后的名字重试并记为**降级 + 警告**（T-B3 待验证）；
  4. **不得静默丢弃附件**（静默丢弃 = 打开时表现正常、运行时缺件）。
- golden：fixture `02-nested` 的 `head-skin-a`（`path = "head"`）→ 解析到 region `head`。

### 6.10 mesh UV（提案；Task 4 范围外）

- 规则（供后续任务）：源 UV 为 **regionUVs**（`0..1`，归一化到 region）—— 与 ADR-0003 C5a 一致，**无需换算**。
- 证据：语料 **0 例** UV 越出 `[0,1]`；三角索引越界 **0 例**。
- 反证价值：若源是未归一化的像素 UV，必然出现大量越界值；实测 0 → 归一化判定成立。

### 6.11 非有限浮点（Back Task §2.1 新增，**强制规则**）

源二进制里的 `float32` 可能是 **NaN / ±Inf**（损坏文件、篡改、或导出器缺陷）。
这类值一旦进入 Domain，会污染求值、序列化与 diff，且极难定位。

**规则**：

1. 读取源 `float32` 后**立即**用 `std::isfinite` 校验；
2. 出现 NaN / ±Inf，**或**任何换算后仍非有限的派生值 → **拒绝整个候选**；
   **不得**只丢弃该字段、不得替换成 0、不得静默钳位；
3. 诊断码（稳定）：`SKELETON_FLOAT_NON_FINITE`；
4. 诊断必须携带 **段名 + 字节偏移**（`sourceLocation`），否则用户无法回到源文件定位；
5. 该检查对**所有**浮点字段生效（骨骼变换、附件参数、约束 mix、偏移等），
   不只动画帧。

**验收要求**（Back Task §2.1）：fixture 与阶段 B 验收必须覆盖
① 非有限 float 被拒绝、② 诊断带字节偏移、③ 失败原子性（无部分提交）。
对应 fixture：`docs/skeleton/fixtures/05-non-finite-float.skel.bytes`（见 `fixtures/README.md`）。

---

## 7. ID 与稳定键（已定案 D25）

### 7.1 铁律

- **不得使用源数组下标作为持久化 ID**（Task 4 §2.2.4）。
- **持久化对象一律由宿主分配随机 ULID**（D25 / ADR-0004 方案 A）。
  换机器、换进程、重复导入都得到不同 ID；ID 不携带任何语义。

### 7.2 源内容 hash 的角色

本节中的 `contentHash` 专指**宿主/插件对 `.skel.bytes` 全文计算的 SHA-256**
（`sourceRef.contentHash`），**不是** Spine 二进制头里的 `hash` 字段 ——
后者是 `sourceRef.sourceHeaderHash`，两者见 §8.3。

| 用途 | 是否允许 |
|---|---|
| `sourceRef.contentHash` 作**来源元数据** / 重复导入**提示** | ✅ 允许 |
| 参与 ID 派生（`ID = H(hash, key)` 之类） | ❌ **禁止**（D25） |
| 作为"是否已导入过"的判据并**隐式复用**已有对象 | ❌ **禁止**；重复导入默认创建副本 |

**去重提示不得隐式复用 ID**：宿主可以提示"该源似乎已导入过 N 次"，
但用户选择"再导入一份"时必须得到**全新的 ULID**，不得把已有对象的 ID 悄悄接到新副本上。

### 7.3 稳定键的用途（仅限单次导入内）

稳定键**只**用于一次导入过程中，把源侧引用（`parentIndex`、`boneIndex`、
附件名、skin/slot 组合等）解析到**本次已分配的 ID**。导入结束即失效。

| 对象 | 稳定键 | 依据 |
|---|---|---|
| Bone | `bone:<name>` | 语料骨骼名**零重名**（1113/1113） |
| Slot | `slot:<name>` | 语料插槽名**零重名**（1113/1113） |
| Skin | `skin:<name>` | 语料 **0 个多皮肤单位**（1113 全为 1 个 `default`） |
| Attachment | `skin:<skin>/slot:<slot>/att:<name>` | 语料同 slot 内附件名**零重名** |
| Constraint | `ik:<name>` / `tc:<name>` | 约束名可重复（Spine 允许）→ 需消歧 |

**重名消歧**：稳定键不是格式保证（Spine 允许重名）。规则：第 1 次出现用原名，
第 2 次及以后用 `<name>#<出现序号>`，并产出**警告**诊断（不拒绝 —— 源数据合法）。

### 7.4 重复导入与未来的显式更新

- **默认行为：创建副本**（D25）。每次导入得到一套全新 ULID。
- **未来**若实现"显式更新/重导并沿用 ID"：必须基于**已持久化的 source-key → ID 映射**，
  且必须由用户显式触发；**不得**对当前内容重新 hash 派生 ID。
- 该持久化映射是否入库、何时入库，属后续任务；本任务**不实现**、不预留字段。

---

## 8. 未知 / 不支持 / 诊断契约

### 8.1 三分法（Task 4 §2.2.6）

| 类别 | 定义 | 工程内是否保留 |
|---|---|---|
| **语义映射** | 有明确 canonical 对应且**在本任务范围内**（§5.6、§10.1） | 是，作为可编辑数据 |
| **不支持且有诊断** | 源里有、但本任务语义不承载（动画时间线、mesh/约束参数、path 约束、boundingbox、sequence index） | **否**。导入时产出诊断；工程内不留任何等价物 |
| **待验证** | 语义未定或有歧义（§11.2） | 否，直到定案 |

### 8.2 明确禁止的做法

**不得**把"不支持"的内容以 Spine 原始字节或私有 JSON 片段塞进工程
（即使是放在未知字段里）。这正是 `项目规范.md` §2.3.6 禁止的
「把 Spine/DragonBones 序列化作为隐藏主数据或内部中转格式」。
同理，**不得**用未知字段机制绕过 §4 的 v2 裁定去透传骨架。

### 8.3 工程内允许保存的源信息（只有元数据；整个对象可选）

`Skeleton.sourceRef` **是整个可选对象**：

| 情形 | 行为 |
|---|---|
| 省略 | 合法。表示"原生创建，无来源信息"。不产生诊断 |
| 存在 | 按下表逐字段校验；缺必需字段 → 拒绝 |
| 为 `null` | **不合法** → 拒绝。用「省略」表达"无来源"，不用 `null` |

理由：Loongbones 的原生数据也必须能创建**没有外部来源**的骨架；强制 `sourceRef`
会让原生创建的数据无处可填，或诱使实现者伪造来源信息（B0 退回补项，2026-10-20）。

对象存在时的字段：

| 字段 | 说明 |
|---|---|
| `formatId` | 如 `spine.binary` |
| `formatVersion` | 如 `3.2.01` |
| `sourceHeaderHash` | **可选**。Spine 二进制头里的 `hash` 字段原文（源自带标识，不做任何哈希运算、不截断）；缺失则省略 |
| `contentHash` | **由宿主/插件对源文件计算**：SHA-256，覆盖 `.skel.bytes` **全文**。**仅**作来源元数据 / 重复提示（§7.2） |
| `sourceUnit` | 源里的逻辑单位（相对引用名），**不含绝对路径、不含盘符/用户名** |
| `importedBy` | 插件 id + 插件版本（供诊断与来源记录，D28） |

> **`sourceHeaderHash` ≠ `contentHash`（Back Task §5.1）**：前者是**源自带**的头字段
> （无算法、原文保存），后者是**我们读取文件后算的** SHA-256 全文摘要。
> 二者来源与算法都不同，**不得**合并、不得把前者"原样"映射为后者。

**不保存**：源字节、源绝对路径、用户名、任何游戏资产本体。

**伴随文件摘要不持久化**：`.atlas.txt` / `.png` 的 SHA-256 **不写入工程**。
它们只出现在 `sample-manifest.md` §1/§1b（按页分别计算）与探针输出中，用于语料锁定与复跑；
与 `contentHash`、`sourceHeaderHash` 均非同义词。
若将来要持久化，必须另起字段名（如 `sourceRef.companionDigests.atlasTxt` / `.png`），
并分别注明覆盖范围。

---

## 9. 校验与诊断

### 9.1 必须拒绝整个候选（而非警告）

| 编码（提案） | 触发 |
|---|---|
| `SKELETON_BONE_PARENT_OUT_OF_RANGE` | `parentIndex` 越界 |
| `SKELETON_BONE_PARENT_FORWARD_REF` | `parentIndex >= 自身下标`（含自环） |
| `SKELETON_BONE_CYCLE` | 父链成环 |
| `SKELETON_SLOT_BONE_UNRESOLVED` | slot 的骨骼引用不可解析 |
| `SKELETON_DRAWORDER_NOT_PERMUTATION` | `drawOrder` 与 `slots[]` 不互为排列 |
| `SKELETON_SKIN_SLOT_OUT_OF_RANGE` | skin 条目的 slot 索引越界 |
| `SKELETON_ATTACHMENT_REGION_UNRESOLVED` | 附件 `path ?? name` 在图集里找不到（§6.9） |
| `SKELETON_REGION_OUT_OF_PAGE` | region 矩形越出页声明尺寸 |
| `SKELETON_MESH_INDEX_OUT_OF_RANGE` | 三角索引越界（提案；Task 4 范围外，供后续任务） |
| `SKELETON_UNKNOWN_ATTACHMENT_TYPE` | 未知附件类型字节 |
| `SKELETON_SKIN_ENTRY_DUPLICATE` | 同一 Skin 内 `(slotId, attachmentName)` 重复 |
| `SKELETON_FLOAT_NON_FINITE` | 源浮点为 NaN / ±Inf，或派生值非有限（§6.11） |
| `SKELETON_UNSUPPORTED_SOURCE_VERSION` | 源格式版本不在 `3.2.01` |

### 9.2 诊断形状

`{ code, severity, message, location (canonical 侧 JSON Pointer), sourceLocation (段名 + 字节偏移) }`。
**两侧都必须给**：只有 canonical 侧用户回不到源文件；只有源侧用户看不出落在哪个对象上。

### 9.3 拒绝时的排序

先 `Format` 再 `Domain`；Domain 内按 **document order**（先出现先报），
不按严重度排序 —— 保证可复现。

### 9.4 插件版本不兼容（已定案 D28）

- `sourceRef.importedBy.pluginVersion` 高于当前已注册插件 → **显式诊断 + 拒绝自动重导**，
  不得猜测、不得降级导入。
- **不提供插件版本迁移钩子**：插件升级**不自动改写**已有 canonical 数据。
- 更新/重导若实现，必须是**用户显式触发**，并遵守 §7.4 的 source-key → ID 映射规则。

---

## 10. 支持矩阵

### 10.1 Task 4 阶段 B 实现范围（本任务的"首批支持"仅指这些）

| 能力 | 分类 | 语料证据 |
|---|---|---|
| 骨架元数据（导入时产出 `sourceRef`：源格式 id / 版本 / 头 hash / 文件 SHA-256；原生创建时省略） | 实现范围 | 1113/1113 `3.2.01`；头 hash 不同值 1107；nonessential true 仅 4 例 |
| 骨骼层级 + 局部变换（含 shear、继承布尔） | 实现范围 | 骨 72256 / 槽 42955（Full）；shear 时间线 189120 |
| 插槽（boneIndex / color / blend） | 实现范围 | 42955 槽 |
| 绘制顺序（静态） | 实现范围 | slot 数组顺序 |
| Skin（含 `default`） | 实现范围 | 1113 皮肤（**全为单皮肤**） |
| Region 附件 | 实现范围 | 27459（Full） |
| Atlas / AtlasRegion / AtlasPage 映射与校验 | 实现范围 | 页 1111、region 43704；rotate true 17734；trim 0 |
| AtlasPage 缩放因子 | 实现范围（页层） | 1107/1111 页声明尺寸 ≠ PNG 尺寸，判据支持 H2 |

### 10.2 不支持且有诊断（不保存源片段）

本任务**不实现**，导入时产出诊断，工程内**不留任何等价物、不保存任何源片段/源字节**
（`项目规范.md` §2.3.6）。

| 能力 | 分类 | 语料证据 |
|---|---|---|
| **动画（全部时间线）** | 不支持且有诊断 | 动画 16046、时间线 4954836（Full） |
| **drawOrder 时间线** | 不支持且有诊断 | 14989 |
| **deform 时间线** | 不支持且有诊断 | 335817 |
| **事件（Event / 事件时间线）** | 不支持且有诊断 | 事件定义 1、事件时间线 0 |
| **mesh / 加权 mesh / linked mesh** | 不支持且有诊断（模型提案见 §5.4，实现归后续任务） | 2981 / 17271 / 2 |
| **IK / transform 约束参数** | 不支持且有诊断（模型提案见 §5.5，实现归后续任务） | 2707 / 208 |
| **path 约束** | 不支持且有诊断 | 0（无样本可验证） |
| **boundingbox / clipping / path 附件** | 不支持且有诊断 | 0 |
| **图集 `index`（sequence）** | 不支持且有诊断 | 0 例非 -1 |
| **多皮肤换装 / 皮肤混合** | 不支持且有诊断 | 语料 0 个多皮肤单位 → **无真实样本可验证**（T-B4） |

> 若将来要为"解析了多少、跳过多少"保留统计，必须**单独定义并实现一个不含源序列化的
> 解析统计字段**（命名、覆盖范围、是否持久化），不得借"保留源片段"之名回退到未知字段透传。

### 10.3 拒绝（导入即失败）

见 §9.1 全部条目。

### 10.4 待验证

见 §11.2。

> **注意**：上表"不支持且有诊断"不是"以后一定支持"，只是"本任务不实现且会明确告知"。
> 已裁定 **D24/D25/D26/D27/D28** 的事项**不在此表重复**。

---

## 11. 待验证与风险（已无待负责人裁定项）

### 11.1 待验证（含责任阶段）

| 编号 | 项 | 现状 | 责任阶段 |
|---|---|---|---|
| **T-A1** | rotation ≠ 0 与图集页缩放的复合（`DECISIONS.md` V2 的另一半） | 承载层已定（页层），复合行为未验证；语料 rotate true 占 40.6%，必须覆盖 | 阶段 B golden |
| **T-A2** | `inheritRotation` / `inheritScale` 布尔的实际分布与求值语义 | 语料未统计 | 阶段 B golden |
| **T-B1** | `pma: true` 时的颜色处理 | 语料 0 例 | 后续任务（图集扩展时） |
| **T-B2** | 图集 `split` / `pad` | 语料 0 例 | 后续任务 |
| **T-B3** | 附件名尾随空格的降级匹配是否接受 | 语料 4 例 | 阶段 B（与负责人确认策略） |
| **T-B4** | 多皮肤 / 多页图集 | 语料 0 例，只能用合成 fixture | 后续任务（mesh/皮肤扩展时） |
| **T-B5** | `deform offset` 形态分布（`DECISIONS.md` V8 残留） | 仍未统计 | 后续任务（动画时） |

### 11.2 风险登记

| 风险 | 说明 | 缓解 |
|---|---|---|
| 输入源是逆向工程数据，不是正版导出 | 语料来自第三方导出，字段解释可能有系统性偏差 | 不联网、不依赖非署名来源；结论只基于本仓库可复跑的实测数字 |
| 语料**无**多皮肤、无多页图集、无 path 约束、无 boundingbox | 这些能力无法用真实语料验证 | 明确标记"无真实样本"，用合成 fixture 覆盖结构层，语义层待验证 |
| 图集页与 PNG 分辨率不一致 | 若误用 PNG 尺寸当 region 坐标空间，所有附件会错位 | §6.7 判定为页声明空间；golden 必测 |
| Task 4 范围内能力少，容易被读成"骨架已支持" | 提案（§5）与实现范围（§5.6/§10.1）不等价 | §1.1/§5.6/§10 三处显式区分；文档标题不用"已支持" |
| 非有限浮点在真实语料中未出现 | 无法用真实样本证明该规则必要 | 规则按损坏输入的最坏情况定义，用合成 fixture 覆盖（§6.11） |

---

## 12. 证据与可复跑性

| 项 | 值 |
|---|---|
| 语料口径 | Full 1113 / primary 963；来自只读语料根（本地配置，不入仓库） |
| 探针（4 个脚本） | `tools/skeleton-probe/probe.mjs`、`png-page-probe.mjs`、`make-fixture.mjs`、`emit-samples.mjs` |
| 复跑 | 见 `tools/skeleton-probe/README.md`；输出只含相对路径 + hash + 统计 |
| 纪律 | 未移动/改写/删除/复制任何游戏文件；未把游戏资产写入仓库；未联网 |
| 与 v1 RFC 的关系 | 本文件是 v1 之上的**提案**；未修改 `docs/format/native-project-v1-rfc.md` |
| v2 schema 冻结 | [`docs/format/native-project-v2-skeleton.md`](../format/native-project-v2-skeleton.md)（B0 产出：完整 JSON 示例、字段必需性/默认值、枚举字符串、ID 规则、未知字段锚点、能力、迁移、错误码、资源落地策略、诊断等级） |
