# Spine 3.2.01 → Loongbones canonical 字段映射表（返修稿）

- 文档状态：**返修稿**；与 `skeleton-domain-rfc.md` §5/§6、`plugin-host-rfc.md` 配套
- 返修日期：2026-10-20
- 源格式口径：`3.2.01` 二进制（语料 **1113/1113** 全为该版本），伴随 libGDX `.atlas.txt`
- 字节布局依据（只读参照）：旧项目 `converter/src/read/binary32.ts`、本仓库 `docs/mapping-spine32-to-native.md`
- 证据：每行的「语料实测」列来自 `tools/skeleton-probe/` 的只读扫描（复跑方式见其 README）

> **本表是映射契约，不是实现。** 阶段 B 的产品读取器按批准的 schema 契约**独立实现**，
> 不复用旧项目代码、不引入 Spine Runtime。

### 三层范围（必须分清，Back Task §2.3）

本表描述的是**源字段 → canonical 的解析/证据映射**。它**不等于**实现范围。
每一行只能落在下列三类之一，且本表显式标注：

| 层 | 含义 | 标注 |
|---|---|---|
| **A. 解析/证据映射** | 源里确实有这个字段，且已实测其分布 | 「语料实测」列 |
| **B. 未来 canonical 模型提案** | canonical 侧将来会长这个字段，但**本任务不实现** | 标记「范围外」 |
| **C. Task 4 实际实现范围** | 本任务阶段 B 真的要做 | 见 §11.1 |

> 简言之：**源里有 ≠ canonical 会有 ≠ 本任务会实现**。
> mesh / 加权 mesh / linked mesh、IK / transform 约束参数属于**层 B**，
> 本任务（层 C）**不实现**，不得称为"首批支持"。

---

## 1. 头部（skeleton）

| 源字段 | 类型 | → canonical | 转换规则 | 语料实测 |
|---|---|---|---|---|
| `hash` | 长度前缀字符串 | `sourceRef.sourceHeaderHash`（**可选**） | 源内字符串**按原文保存**（不截断、不重算、不改写）；缺失/为空则省略该字段。**这是源文件自带的标识，不是我们对文件算的摘要** | 不同 hash 1107 / 1113 文件 |
| （无对应源字段） | — | `sourceRef.contentHash` | **由宿主/插件读取源文件后计算**：算法 **SHA-256**，覆盖范围 = **`.skel.bytes` 全文**。**不来自** Spine 头 `hash` | — |
| `version` | 字符串 | `sourceRef.formatVersion` | 原样；非 `3.2.01` → 拒绝 | **全部 `3.2.01`** |
| `width` / `height` | float32 | 提案中不落字段 | 仅编辑器参考 | 无 canonical 语义 |
| `nonessential` | bool | — | 决定后续是否有 `imagesPath` 与附件颜色段 | true **4** / false **1109** |
| `imagesPath` | string? | — | 仅当 `nonessential` 为真时出现；**不用于**图集解析（§9） | 非空 **4** |

> **两个 hash 是不同东西，禁止混为一字段（Back Task §5.1）**：
>
> | 字段 | 来源 | 算法 | 覆盖范围 | 必填 |
> |---|---|---|---|---|
> | `sourceRef.sourceHeaderHash` | **Spine 二进制头里的 `hash` 字段**（源自带） | 无（原文字符串，不做任何哈希运算） | 仅该头字段本身 | 可选 |
> | `sourceRef.contentHash` | **宿主/插件读取的 `.skel.bytes` 文件**（我们算） | SHA-256 | `.skel.bytes` **全文** | 是（来源元数据） |
>
> - **禁止**把头部 `hash` "原样"映射为 `contentHash`：二者来源与算法都不同，
>   同名会造成"这是我们对文件算的摘要"的误读。
> - `.atlas.txt` / `.png` 的摘要**不写进工程**，只存在于
>   [`sample-manifest.md`](sample-manifest.md) §1/§1b（按页分别计算）与探针输出中；
>   它们同样**不是** `contentHash`。
> - 样本 manifest 里的 SHA-256 是**外部复现锁定值**（与 `contentHash` 同算法同范围，
>   但由探针离线计算，不经过导入路径）。

## 2. 骨骼（bones）

| 源字段 | → canonical | 转换规则 | 语料实测 |
|---|---|---|---|
| `name` | `Bone.name` + 稳定键 `bone:<name>` | 原样 | 骨骼名 **零重名**（1113/1113） |
| `parentIndex`（i>0 才有） | `Bone.parentId` | `-1` → `null`；否则必须 `< i` | 越界 **0**、前向引用 **0**、自环 **0**、根 **1113** |
| `rotation` | `Bone.rotationDeg` | 度，直接映射（ADR-0003 C2） | — |
| `x` / `y` | `Bone.x` / `Bone.y` | Y-up，无符号翻转（ADR-0003 C1） | — |
| `scaleX` / `scaleY` | `Bone.scaleX` / `Bone.scaleY` | 直接 | — |
| `shearX` / `shearY` | `Bone.shearXDeg` / `shearYDeg` | **必须建模** | 语料 189,120 条 shear 时间线 |
| `length` | `Bone.length` | 编辑器辅助，不参与求值 | — |
| `inheritScale` | `Bone.inheritScale` | 布尔，直接 | 分布未统计（T-A2） |
| `inheritRotation` | `Bone.inheritRotation` | 布尔，直接；**不**伪造 3.6+ 的 transform 枚举 | 同上 |

## 3. 约束

### 3.1 IK

| 源字段 | → canonical | 语料实测 |
|---|---|---|
| `name` | `IkConstraint.name` + 稳定键 `ik:<name>` | **2707**（Full）/ 2481（primary） |
| `boneIndices[]` | `IkConstraint.boneIds[]`（有序链） | 索引越界 **0** |
| `targetIndex` | `IkConstraint.targetBoneId` | 越界 **0** |
| `mix` | `IkConstraint.mix`（0..1） | — |
| `bendPositive` | `IkConstraint.bendPositive` | — |

### 3.2 Transform（3.0 新增；本语料**无** path 约束）

| 源字段 | → canonical | 语料实测 |
|---|---|---|
| `name` | `TransformConstraint.name` + 稳定键 `tc:<name>` | **208**（Full）/ 160（primary） |
| `boneIndex` | `TransformConstraint.boneId` | 越界 **0** |
| `targetIndex` | `TransformConstraint.targetBoneId` | 越界 **0** |
| `rotateMix` / `translateMix` / `scaleMix` / `shearMix` | 同名 | — |
| `offsetRotation` / `offsetX` / `offsetY` / `offsetScaleX` / `offsetScaleY` / `offsetShearY` | 同名 | — |

> 3.2 的 transform 约束**只绑一根骨**（不是列表）。

### 3.3 Path

语料 **0 例** → 不支持且有诊断。

## 4. 插槽（slots）

| 源字段 | → canonical | 转换规则 | 语料实测 |
|---|---|---|---|
| `name` | `Slot.name` + 稳定键 `slot:<name>` | 原样 | 插槽名 **零重名**（1113/1113） |
| `boneIndex` | `Slot.boneId` | 必须指向本骨架内骨骼 | 越界 **0** |
| `color`（8-bit RGBA） | `Slot.color` | `/255` → `0..1` 浮点（ADR-0003 C8） | — |
| `attachment`（可为 null） | `Slot.attachmentName` | 空 = 不显示 | — |
| `blend`（0..3） | `Slot.blendMode` | `0 normal / 1 additive / 2 multiply / 3 screen`；越界 → 诊断并回落 normal | 越界 **0** |

**绘制顺序**：无源字段；slot 数组顺序即绘制顺序 → `Skeleton.drawOrder`（slot ID 有序数组）。

## 5. 皮肤与附件

字节序（**每个 skin 条目两段字符串**，易错点）：

```
skin: slotCount
  per slot:  slotIndex, attCount
    per att: skinKey(string), attachmentName(string|null), typeByte, <类型专属字段>
```

- `attachmentName` 为 `null` 时回退到 `skinKey`（语料中绝大多数如此）。
- `skinKey` 为 `null` 表示「该 slot 在此皮肤下不显示附件」→ **空引用**，不是缺失素材。

### 5.1 Region 附件（`typeByte = 0`）

| 源字段 | → canonical | 语料实测 |
|---|---|---|
| `path`（可为 null） | 参与 region 查找（§10） | 显式 path **84** 个，且 **84/84** 与附件名不同 |
| `rotation` | `RegionAttachment.rotationDeg` | 非零 **24351 / 27459**（88.7%） |
| `x` / `y` | 同名 | — |
| `scaleX` / `scaleY` | 同名 | ≠ 1 共 **744** 例 |
| `width` / `height` | 同名 | — |
| `color` | 同名（`/255`） | — |

### 5.2 Mesh 附件（`typeByte = 1/2/3/4`）

| 源字段 | → canonical | 语料实测 |
|---|---|---|
| `uvs` | `MeshAttachment.uvs`（**regionUVs**，`0..1`） | 越出 `[0,1]` **0 例** → 归一化判定成立（ADR-0003 C5a） |
| `triangles` | `MeshAttachment.triangles`（顶点索引，非字节偏移） | 索引越界 **0 例** |
| `vertices` / `weights` / `bones` | 同名（加权绑定静态数据） | 加权 mesh **17271**（Full） |
| `hull` | 提案中不落字段（编辑器辅助） | — |
| `parent`（linked mesh，type 4/5） | `MeshAttachment.parentName` | linked mesh **2**（Full） |

### 5.3 其它类型

`boundingbox` / `clipping` / `path` 附件：语料 **0 例** → 不支持且有诊断。

## 6. 事件（events）

| 源字段 | → canonical | 语料实测 |
|---|---|---|
| `name` / `int` / `float` / `string` | **不支持且有诊断** | 事件定义 **1**；事件时间线 **0** |

## 7. 动画（animations）

| 时间线 kind | → canonical | 语料实测（Full / primary） |
|---|---|---|
| `rotate` / `translate` / `scale` / `shear` | **不支持且有诊断** | 骨骼时间线合计 **3,822,032 / 3,608,097** |
| `attachment` / `color` | **不支持且有诊断** | 插槽时间线 **740,697 / 698,760** |
| `deform` | **不支持且有诊断** | **335,817 / 316,722** |
| `ik` | **不支持且有诊断** | **38,680 / 36,408** |
| `drawOrder` | **不支持且有诊断** | **14,989 / 14,136** |
| `transform`（约束） | **不支持且有诊断** | **2,621 / 2,200** |
| `event` | **不支持且有诊断** | **0 / 0** |

动画总计：动画 **16,046 / 15,067**，时间线 **4,954,836 / 4,676,323**。

## 8. 皮肤数量

语料 **0 个多皮肤单位**（1113 个文件各有且仅有 1 个 `default` 皮肤）
→ **多皮肤换装无法用真实语料验证**（T-B4），只能靠合成 fixture 覆盖结构层。

## 9. 图集（`.atlas.txt` → `Atlas` / `AtlasPage` / `AtlasRegion`）

| 源（页头） | → canonical | 转换规则 | 语料实测 |
|---|---|---|---|
| 页文件名 | `AtlasPage` → `TextureAsset` | 按**源内相对引用**解析，不用 `imagesPath` | 页 **1111** |
| `size: W,H` | `AtlasPage.width/height` | **权威坐标空间**（§10.2） | 存在 **1111 / 1111**；与 PNG IHDR 一致仅 **4 / 1111** |
| `format` | — | 元信息 | **全为 `RGBA8888`** |
| `filter` / `repeat` | 提案中不落字段（渲染侧采样参数） | — | — |
| `pma` | — | 语料**全部未声明** → 按非预乘处理；遇 `true` 需显式处理（T-B1） | true **0** / false **0** / 缺省 **1111** |

| 源（region） | → canonical | 转换规则 | 语料实测 |
|---|---|---|---|
| region 名 | `AtlasRegion.name` | 原样 | region **43,704** |
| `xy: x,y` | `AtlasRegion.packedX/packedY` | 页坐标空间 | 越出页声明尺寸 **0** |
| `size: w,h` | `AtlasRegion.packedWidth/Height` | 页坐标空间 | — |
| `orig: w,h` | `AtlasRegion.originalWidth/Height` | 原图尺寸 | — |
| `offset: x,y` | `AtlasRegion.offsetX/Y` | trim 偏移 | **trim 0 例**（`size == orig` 100%） |
| `rotate: true/false` | `AtlasRegion.rotation`（`none` / `90`） | `true` → `"90"` | true **17,734** / false **25,970** / 其它写法 **0** |
| `index` | **不落入 v1**（`AtlasRegion` 无此字段） | 不支持且有诊断 | 非 `-1` **0 例** |
| `split` / `pad` | **不落入 v1** | 待验证 T-B2 | 未出现 |

**页与纹理的像素尺寸**：`AtlasPage.width/height` = 图集坐标空间；
`TextureAsset.pixelWidth/Height` = PNG 实际像素（语料 PNG 恒为 2 的幂）。二者不等时，
缩放因子在**页层**应用一次（详见 `skeleton-domain-rfc.md` §6.7）。

## 10. 附件 → region 查找与坐标空间

### 10.1 查找规则

键 = **`path ?? name`**（Spine 规则）。

| 口径 | 命中 | 未命中 |
|---|---|---|
| `path ?? name` | **47,705** | **4** |
| 仅按附件名 | 47,632 | 77 |

4 例未命中根因：**附件名带尾随空格**（如 `"head_back "` vs 图集里的 `head_back`）。
契约：精确匹配 → 未命中**必须产出诊断** `SKELETON_ATTACHMENT_REGION_UNRESOLVED`；
可选按 `trim()` 重试并记降级警告；**不得静默丢弃**。

### 10.2 坐标空间（关键，已实测判定）

- 现象：页声明 `size` 普遍小于伴随 PNG 的 IHDR 尺寸（**1107 / 1111** 不一致）。
- 判据（`png-page-probe.mjs`，只读 alpha，不落地图像）：
  非透明像素外接矩形贴合 PNG 尺寸；「落在页声明框内」的不透明像素占比 0.22–0.30，
  与「PNG 是更高分辨率同一张图」的预测值 0.25–0.27 吻合，与「PNG 是补齐 padding」的
  预测值 ≈1.0 相差数量级。判定分布：该假设 **26 / 0 / 3**（支持 / 反对 / 不确定，另有 1 例不支持的 PNG 形态）。
- 结论：**region 坐标空间 = 页头声明的 `size`**；PNG 为更高分辨率纹理；
  缩放 = `pixelWidth / page.width`，在**页层**应用。

## 11. 支持矩阵（源字段级，按三层范围拆分）

### 11.1 层 C：Task 4 实际实现范围（本任务的"首批支持"仅指这些）

| 能力 | 源字段 | 语料实测 |
|---|---|---|
| 骨架元数据 | `hash` / `version` / `width` / `height` / `nonessential` / `imagesPath` | 1113/1113 `3.2.01`；nonessential true 4 / false 1109；imagesPath 非空 4 |
| 骨骼层级 + 局部变换 | `name` / `parentIndex` / `x` / `y` / `rotation` / `scaleX` / `scaleY` / `shearX` / `shearY` / `length` / `inheritScale` / `inheritRotation` | 骨 72256（Full）；越界 0、前向引用 0、自环 0；shear 时间线 189120 |
| 插槽 | `name` / `boneIndex` / `color` / `attachment` / `blend` | 槽 42955（Full）；boneIndex 越界 0；blend 越界 0 |
| 静态绘制顺序 | （无源字段，slot 数组顺序） | slot 名零重名 |
| 皮肤 | skin 段：`slotIndex` / `attCount` / `skinKey` / `attachmentName` / type byte | 1113 皮肤，**全为单皮肤** |
| Region 附件 | `path` / `rotation` / `x` / `y` / `scaleX` / `scaleY` / `width` / `height` / `color` | 27459（Full）；rotation 非零 24351；scale≠1 744 |
| 图集页与 region | 页头 `size`/`format`/`filter`/`repeat`/`pma`；region `xy`/`size`/`orig`/`offset`/`rotate`/`index` | 页 1111、region 43704；rotate true 17734；trim 0；index 非 -1 0；pma 全未声明；format 全 RGBA8888 |

### 11.2 层 B：未来 canonical 模型提案（本任务**不实现**）

| 能力 | 源字段 | 语料实测 | 去向 |
|---|---|---|---|
| Mesh 附件（静态参数） | `uvs` / `triangles` / `vertices` / `hull` / `edges` | mesh 2981（Full） | 后续任务 |
| 加权 mesh | `weights` / `bones` | 17271（Full） | 后续任务 |
| Linked mesh | `parent` / `skin` / `inheritDeform` | 2（Full） | 后续任务 |
| IK 约束参数 | `name` / `boneIndices[]` / `targetIndex` / `mix` / `bendPositive` | 2707（Full） | 后续任务 |
| Transform 约束参数 | 见 §3.2 全列 | 208（Full） | 后续任务 |
| Path 约束 | — | 0 | 后续任务 |

### 11.3 不支持且有诊断（本任务不实现，导入时告知）

全部动画时间线（rotate/translate/scale/shear/attachment/color/deform/ik/drawOrder/transform/event）、
事件定义、path 约束、boundingbox/clipping/path 附件、图集 `index`/`split`/`pad`、多皮肤换装。

### 11.4 拒绝（导入即失败）

版本非 `3.2.01`、骨骼父索引越界/前向引用/成环、slot 骨骼不可解析、skin slot 索引越界、
`drawOrder` 与 `slots` 不互为排列、region 越出页、三角索引越界、未知附件类型字节、
**源浮点为 NaN / ±Inf**（诊断 `SKELETON_FLOAT_NON_FINITE`，见 §12）。

### 11.5 待验证

rotation≠0 与页缩放复合（T-A1）、继承布尔分布（T-A2）、`pma:true`（T-B1）、
`split`/`pad`（T-B2）、尾随空格降级（T-B3）、多皮肤/多页图集（T-B4）。

---

## 12. 非有限浮点（Back Task §2.1 新增，强制）

源二进制里的 `float32` 可能是 **NaN / ±Inf**（损坏文件、篡改、导出器缺陷）。
这类值一旦进入 Domain，会污染求值、序列化与 diff，且极难定位。

| 要求 | 内容 |
|---|---|
| 校验时机 | 读取源 `float32` 后**立即**用 `std::isfinite` 校验 |
| 触发即失败 | 出现 NaN / ±Inf，或任何换算后仍非有限的派生值 → **拒绝整个候选** |
| 禁止的做法 | 只丢弃该字段、替换成 0、静默钳位 |
| 诊断码（稳定） | `SKELETON_FLOAT_NON_FINITE` |
| 诊断内容 | 段名 + **字节偏移**（`sourceLocation`） |
| 覆盖范围 | **所有**浮点字段（骨骼变换、附件参数、约束 mix、偏移等），不只动画帧 |
| 验收 | fixture `05-non-finite-float` + 阶段 B 覆盖 ①拒绝 ②字节偏移 ③失败原子性 |

---

## 13. 证据索引

| 结论 | 证据来源 |
|---|---|
| 版本 / 解析成功率 / 计数 | `tools/skeleton-probe/out/skeleton-probe-report.md` §1、§7 |
| 引用闭合 / 名称唯一性 | 同上 §2、§3 |
| 附件 path 语义 | 同上 §4 |
| 图集形态（rotate/trim/pma/format） | 同上 §5 |
| 附件→region 查找率 | 同上 §6 |
| 坐标空间判定 | `tools/skeleton-probe/out/atlas-page-space-verdict.json` |
| 合成 fixture 与预期结果 | `docs/skeleton/fixtures/README.md` |
