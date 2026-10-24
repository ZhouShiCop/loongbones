# 原生格式 v3（第二批骨架类型增量）Schema 提案

- 文档状态：**提案（未冻结）** —— Task 5 阶段 A 交付物之一，**获批前不得实现**（Task 5 §3 A6）
- 日期：2026-10-22
- 上位文档：`docs/format/native-project-v2-skeleton.md`（**v2 冻结稿，本文不改写它**）、
  `docs/format/native-project-v1-rfc.md`、`docs/skeleton/mesh-and-constraint-rfc.md`（A2 模型提案）、
  `docs/skeleton/skeleton-domain-rfc.md`
- 定位：**v3 = v2 + 第二批骨架类型**（mesh / weightedMesh / linkedMesh / IK / transform 约束；
  `boundingbox` 仅占位）。**结构增量扩展，不是重新设计 v2**（D29）
- 裁定依据：**D29**（直接增量修改结构 + 升 `formatVersion = 3`，**不做 capability 协商层**）、
  D22（未知字段锚点）、D25（ID）、D31（`hull`/`edges` 不落）、D32（boundingbox 不实现）

### 证据范围声明

与 v2 同源：字段依据**全部来自 Spine 3.2.01 语料**（Full 1113 / Primary 963）。
**尚未完成 DragonBones 数据对照**，结论只代表「Spine 3.2.01 → Loongbones」这一条路径，
不得表述为跨格式通用结论。

---

## 1. v3 相对 v2 的增量清单

| # | 增量 | 类型 |
|---|---|---|
| 1 | `formatVersion` → `3`、`minimumReaderVersion` → `3` | 版本 |
| 2 | `Attachment` 种类扩展：`mesh` / `weightedMesh` / `linkedMesh` | 新对象（`region` 语义**不变**） |
| 3 | `boundingbox` 附件类型**占位**（有枚举值、无解析） | 占位（D32） |
| 4 | `Skeleton.ikConstraints[]` / `Skeleton.transformConstraints[]` | 新数组（键必须存在，可为空数组） |
| 5 | `OwnerKind` 新增 `kIkConstraint` / `kTransformConstraint` | 未知字段宿主种类 |
| 6 | 未知附件类型 / 未知约束种类 → **显式拒绝 + 可定位诊断** | 读取规则 |

**v3 不改动**：v1 / v2 全部字段的含义、`formatId`、节点/场景/资产/图集 schema、
`RegionAttachment` 字段与语义、`SkinEntry` 的 ID 规则、资源落地策略（v2 §7）、
Reader 安全上限（D20）、未知字段保留机制（D18/D22）。

**不新增 capability**（与 D29 一致）：v3 靠 `formatVersion = 3` 表态，
**不引入** `loongbones.mesh.v1` 之类的能力声明；`capabilities.required` 沿用 v2 的
`loongbones.atlas.v1` + `loongbones.skeleton.v1`，不新增条目。

---

## 2. 完整 JSON 示例（最小 v3）

只展示**本次增量**相关的片段（`project` / 场景 / 图集部分同 v2，省略）：

```json
{
  "formatId": "loongbones.native-project",
  "formatVersion": 3,
  "minimumReaderVersion": 3,
  "capabilities": { "required": ["loongbones.atlas.v1", "loongbones.skeleton.v1"], "optional": [] },
  "project": {
    "skeletons": [
      {
        "id": "01J...",
        "name": "sample",
        "bones": [ { "id": "01J...", "name": "root" }, { "id": "01J...", "name": "hip" } ],
        "slots": [ { "id": "01J...", "name": "body", "boneId": "01J..." } ],
        "skins": [
          {
            "id": "01J...",
            "name": "default",
            "entries": [
              {
                "id": "01J...",
                "slotId": "01J...",
                "attachmentName": "body_mesh",
                "attachment": {
                  "type": "weightedMesh",
                  "regionId": "01J...",
                  "width": 64, "height": 64,
                  "uvs": [0.0, 0.0, 1.0, 0.0, 1.0, 1.0],
                  "triangles": [0, 1, 2],
                  "vertexBindings": [
                    [ { "boneId": "01J...", "x": 0, "y": 0, "weight": 1.2 },
                      { "boneId": "01J...", "x": 10, "y": 0, "weight": -0.2 } ],
                    [ { "boneId": "01J...", "x": 32, "y": 0, "weight": 1.0 } ],
                    [ { "boneId": "01J...", "x": 32, "y": 32, "weight": 0.5 },
                      { "boneId": "01J...", "x": 0, "y": 32, "weight": 0.5 } ]
                  ]
                }
              },
              {
                "id": "01J...",
                "slotId": "01J...",
                "attachmentName": "body_mesh_linked",
                "attachment": {
                  "type": "linkedMesh",
                  "regionId": "01J...",
                  "parentKey": { "slotId": "01J...", "attachmentName": "body_mesh" },
                  "inheritDeform": true
                }
              }
            ]
          }
        ],
        "ikConstraints": [
          { "id": "01J...", "name": "leg", "boneIds": ["01J...", "01J..."],
            "targetBoneId": "01J...", "mix": 1.0, "enabled": true, "bendPositive": true }
        ],
        "transformConstraints": [
          { "id": "01J...", "name": "copy", "boneId": "01J...", "targetBoneId": "01J...",
            "rotateMix": 90.0, "translateMix": 0.0, "scaleMix": 0.0, "shearMix": 0.0,
            "offsetRotation": 0, "offsetX": 0, "offsetY": 1,
            "offsetScaleX": 0.5, "offsetScaleY": 1, "offsetShearY": 0 }
        ]
      }
    ]
  }
}
```

> 示例中 `weight: -0.2` 是**合法**的（D34：负权重＝该绑定对顶点无影响），不是笔误。
>
> 示例中的 `weightedMesh` **没有 `vertices`**：加权 mesh 的几何全部在 `vertexBindings` 里
> （顶点数 = `vertexBindings` 的外层长度），与 §3.1 字段表一致。

---

## 3. 字段表：必需性与默认值

### 3.1 `Attachment`（v3 的四种；`region` 沿用 v2 §3.6，不改）

| 字段 | `mesh` | `weightedMesh` | `linkedMesh` | 说明 |
|---|---|---|---|---|
| `type` | 必需 | 必需 | 必需 | 枚举字符串；未知值 → **拒绝**（§8） |
| `regionId` | 必需 | 必需 | 必需 | 指向 `AtlasRegion`；linked 与父一致 |
| `uvs` | 必需 | 必需 | **不适用** | regionUVs，`0..1`；长度 = 2×顶点数 |
| `triangles` | 必需 | 必需 | **不适用** | 顶点索引，值须 `< 顶点数` |
| `vertices` | 必需 | **不适用** | **不适用** | 非加权：槽骨骼局部坐标；长度 = 2×顶点数 |
| `vertexBindings` | **不适用** | 必需 | **不适用** | 外层下标＝顶点；内层为该顶点的绑定列表 |
| `vertexBindings[][].boneId` | — | 必需 | — | 指向本骨架 `Bone` |
| `vertexBindings[][].x` / `.y` | — | 必需 | — | **该骨骼局部坐标**；源原值 |
| `vertexBindings[][].weight` | — | 必需 | — | **源原值**，可为负（D34） |
| `parentKey` | — | — | 必需 | `{ slotId, attachmentName }`，指向父 mesh / weightedMesh |
| `inheritDeform` | — | — | 必需（默认 `false`） | `true` = 跟随父的 deform |
| `width` / `height` | 可选（默认 0） | 可选（默认 0） | **不适用** | nonessential；语料 100% 出现 |
| `hull` / `edges` | **不落**（D31） | **不落** | **不落** | 导入时计入 `report.unsupported` 账本 |

### 3.2 `IkConstraint`

| 字段 | 必需性 | 说明 |
|---|---|---|
| `id` | 必需 | 随机 ULID（D25） |
| `name` | 必需 | |
| `boneIds` | 必需（≥1） | **有序链** |
| `targetBoneId` | 必需 | |
| `mix` | 必需 | **源原值**，不解释 |
| `enabled` | 必需 | `mix != 0`（D35） |
| `bendPositive` | 必需（默认 `true`） | |

### 3.3 `TransformConstraint`

| 字段 | 必需性 | 说明 |
|---|---|---|
| `id` / `name` | 必需 | |
| `boneId` / `targetBoneId` | 必需 | |
| `rotateMix` / `translateMix` / `scaleMix` / `shearMix` | 必需（默认 0） | **源原值，不解释语义**（D36） |
| `offsetRotation` / `offsetX` / `offsetY` / `offsetScaleX` / `offsetScaleY` / `offsetShearY` | 必需（默认 0） | 同上 |

### 3.4 `boundingbox`（占位）

枚举值 `boundingbox` 保留；**不实现解析、映射、校验与导入**（D32）。
解析到该类型 → 产出「不支持 + 待验证」诊断，不建对象、不留半成品。

---

## 4. ID 规则

- 沿用 D25：持久化 ID 一律随机 ULID；源下标只在单次导入内映射。
- 附件仍是 `SkinEntry` 的内嵌值对象，**不单独分配 ID**（同 v2 §5.3）。
- **约束是骨架级对象，必须有自己的 ID**（`IkConstraint.id` / `TransformConstraint.id`）——
  否则未知字段锚点无法位置无关地绑定。
- LinkedMesh 引用父附件用 `(slotId, attachmentName)` 组合键（父附件无 ID，见 A2 §3）。

---

## 5. 未知字段宿主锚点

沿用 D22 `(kind, 稳定 ID)`，位置无关：

| 对象 | 锚点 |
|---|---|
| 附件（内嵌，无 ID） | `(kSkinEntry, entryId)` + 相对子路径 `/attachment`（同 v2） |
| `IkConstraint` | `(kIkConstraint, constraintId)` |
| `TransformConstraint` | `(kTransformConstraint, constraintId)` |

锚点解析失败（宿主已删除）→ 丢弃该未知字段记录，不报错。

---

## 6. 能力与版本

| 项 | v3 规则 |
|---|---|
| `formatId` | `loongbones.native-project`（不变） |
| `formatVersion` | `3` |
| `minimumReaderVersion` | `3`（写出时不低于 3） |
| `capabilities.required` | 沿用 v2：`loongbones.atlas.v1` + `loongbones.skeleton.v1`，**不新增**（D29） |

### 6.1 读取顺序与错误码（沿用 v2 §6.1，不二选一）

1. `formatId` 不匹配 → `kFormatIdUnsupported`
2. `formatVersion` 超出本 Reader 范围 → **固定 `kFormatVersionUnsupported`**
   （v1 / v2 Reader 读到 3 同样是它，**不做降级打开、不静默丢字段**）
3. `minimumReaderVersion` 高于本 Reader 上限 → **`kFormatTooNew`**（与 v2 冻结稿 §6.1 第 3 步**同名**；
   原稿此处误写为 `kCapabilityUnsupported`，2026-10-22 二次核对已按冻结稿统一）
4. `capabilities.required` 含未知项 → `kCapabilityUnsupported`
5. 之后才进入 JSON 结构 / 字段 / Domain 校验

---

## 7. 迁移契约

### 7.1 迁移链

```
v1 ──migrateV1ToV2()──▶ v2 ──migrateV2ToV3()──▶ v3 （写出统一为 v3）
```

- `migrateV1ToV2()`：**复用 B3 既有实现，不改**。
- `migrateV2ToV3()`：**新增纯函数** —— 不生成 ID、不改旧字段、不碰磁盘、**可重复执行**。

### 7.2 `migrateV2ToV3()` 规则

| 步骤 | 内容 |
|---|---|
| 版本 | `formatVersion` 2 → 3；`minimumReaderVersion` 2 → 3 |
| 骨架 | 每个 `skeleton` 补 `ikConstraints: []` 与 `transformConstraints: []`（键必须存在） |
| 附件 | v2 只有 `region` 附件 ⇒ **逐个不改**；不新增字段、不补默认值 |
| 约束 | v2 无约束对象 ⇒ 结果为空数组 |
| 未知字段 | 完整保留（D18/D22），锚点不变 |
| 幂等 | 对已是 v3 的文档再执行一次 = 无变化（版本相同即跳过） |

---

## 8. 诊断（Format 层）

沿用 D17：格式诊断码归 **Format 层**，不加入 Domain 的 `DiagnosticCode`。

| 情形 | 处置 |
|---|---|
| 未知 `attachment.type`（含未来新增类型） | **拒绝并报可定位码**，不做"当作 region"的降级 |
| 未知约束种类（未来新增 `path` 等） | **拒绝**并报可定位码 |
| `vertexBindings` 下标数与顶点数不一致 | 拒绝 |
| `triangles` 索引越界 / `uvs` 长度不是偶数 | 拒绝（Domain 侧对应 `MESH_*` 码，见 A2 §11） |
| 数值超出 `integer` / `number` 值域 | 拒绝（D23） |

---

## 9. v3 明确不包含

动画（全部时间线）、deform 时间线、事件与事件时间线、`drawOrder` 时间线、
path 约束、clipping 附件、`boundingbox` 的**实现**、多皮肤换装 / 皮肤混合、
`hull` / `edges`。这些维持「不支持且有诊断」，语料规模见 A2 §2。

---

## 10. fixtures 与可复跑性

- B3 的 **v2 fixture 保留为历史 fixture**，不删除、不改写（规范 §4.3 / §4.6）。
- 新增 **v3 fixture**：最小 mesh、加权 mesh（含负权重）、linked mesh、畸形引用（越界）、
  约束（IK + transform）、v2→v3 迁移前后语义比较。
- 预期结果与「哪些只是合成、无真实样本」在 `docs/skeleton/fixtures/README.md` 登记。

---

## 11. 实现同步记录（2026-10-22，阶段 B 的 B2 落地）

本文 §1–§10 是**提案**；本节记录**实现**与提案的差异、以及评审提出的补正，落地后以本节为准。

### 11.1 已实现

| 提案项 | 实现位置 | 状态 |
|---|---|---|
| `formatVersion` / `minimumReaderVersion` → 3 | `format/include/loongbones/format/codec.hpp`（`kFormatVersion` / `kReaderMaxVersion` = 3，`kReaderMinVersion` 保持 1） | ✅ |
| 4 种附件按 `type` 分派 | `format/src/decode.cpp` `decodeAttachment()` / `format/src/encode.cpp` `emitAttachment()` | ✅ |
| 未知 `attachment.type` 显式拒绝 | `decodeAttachment()` → `kJsonTypeMismatch`（不降级为 region） | ✅ |
| `IkConstraint` / `TransformConstraint` 读写 | `decodeIkConstraint()` / `decodeTransformConstraint()` / 对应 emit | ✅ |
| `OwnerKind` 新增 2 项 | `format/include/loongbones/format/project_document.hpp` | ✅ |
| `migrateV2ToV3()` | `format/src/decode.cpp`；**纯函数、幂等、只标记 `sourceVersion`** | ✅（见 11.3） |
| 骨架两个约束键恒存在 | 写出恒写（可空数组）；读取时**缺失视为空数组** | ✅ |

### 11.2 评审 P0：已知键表**按 type 分派**（不得用并集）

实现的已知键表：`kRegionAttachmentKeys` / `kMeshAttachmentKeys` / `kWeightedMeshAttachmentKeys` /
`kLinkedMeshAttachmentKeys` / `kParentKeyKeys` / `kVertexBindingKeys` /
`kIkConstraintKeys` / `kTransformConstraintKeys`。

**理由**：若把 mesh 族的键并进 region 的键表，region 附件里出现的 `uvs` 会被当成"已知键"而
**静默丢弃**，拼错的文档被悄悄存回而不报错。分派后，类型不符的字段一律走**未知字段通道**
（保留原值并回写），既不丢也不误解析。

骨架层同理：`kSkeletonKeys` 必须含 `ikConstraints` / `transformConstraints`，
否则这两个键会被收成未知字段、再被回写成**重复键** —— 多一轮往返即解析失败。

### 11.3 迁移与读取规则的实现细节

- `migrateV2ToV3()` 实际只做**版本标记**：v2 文档的两个约束键在解码时已按"缺失 = 空数组"处理，
  迁移结果与之等价；附件逐个不改（v2 只有 region）。
- 本 Reader 的读取上限是 **3**：`formatVersion = 4` 必须被拒（既有断言已相应更新，
  变更逐条披露于 `Agent_Task/Task5_B1B2_Report.md` §5）。
- 权重含 NaN / ±Inf 时只报 `SKELETON_VALUE_NOT_FINITE`，**不叠加** `MESH_WEIGHT_SUM_INVALID`
  （权重和此时无法判定，避免同一处重复报两条）。

### 11.4 未知字段的嵌套子路径约定（decode 与 encode 逐字一致）

| 位置 | 相对子路径 |
|---|---|
| 附件对象 | `/attachment` |
| linked 的 `parentKey` 对象内 | `/attachment/parentKey` |
| weighted 的某个绑定对象内 | `/attachment/vertexBindings/{i}/{j}` |

约束对象（IK / transform）自身的未知字段挂在**其自身锚点**（`kIkConstraint` / `kTransformConstraint` + 约束 ID），
子路径为空 —— 评审 G2 指出的「约束对象未知字段只写不读」已修正（此前解码侧不收集，锚点无来源）。

### 11.5 未决

`kFormatTooNew` / `kCapabilityUnsupported` 的命名分歧：本文按 v2 冻结稿取 **`kFormatTooNew`**，
**待负责人最终裁定**；实现侧尚未为这两个码新增行为，仅沿用既有格式诊断码。
