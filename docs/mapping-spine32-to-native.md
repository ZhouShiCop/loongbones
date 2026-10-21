# Spine 3.2.01 → Native Model 字段/语义映射（草案）

对应规范：`项目规范.md` §12 交付物 4。
**状态：草案，未经负责人审阅。** 本文只记录"已知什么、不知道什么"，
所有语义判断都标注证据来源；不确定项集中在 §4，**不得当作既定事实**。

数据来源：`tools/corpus-scan/` 对 963 个 primary 单位的解析统计，
行为参照为旧项目自研 Reader（`converter/src/read/binary32.ts`，经 963 文件严格 EOF 验证）
与 Spine Runtime 源码（仅作行为参照，许可证见 `docs/licenses-and-risks.md`）。

---

## 1. 文件容器层

Spine 3.2.01（sdorica 私有分支）一个"单位"是三个文件：

| 文件 | 内容 | Native Model 对应 |
|---|---|---|
| `<name>.skel.bytes` | 二进制骨架：header + bones + slots + ik/transform/path + skins + animations + events | `Skeleton` + `Animation[]` + `Event[]` |
| `<name>.atlas.txt` | libGDX 格式图集：页头（`size`/`format`/`filter`/`repeat`）+ region（`rotate`/`xy`/`size`/`orig`/`offset`/`index`） | `Atlas`（页 + `AtlasRegion[]`） |
| `<name>.png` | 图集页图像 | 外部资源引用（不内嵌进骨架） |

二进制 header 布局（`binary32.ts:90-101`）：

```
hash: string          ← 随机串（如 "goXf9oQdeatoZIH"），不是版本号
version: string       ← 实测全部 "3.2.01"
width: float
height: float
nonessential: boolean
[imagesPath: string]  ← 仅 nonessential 时存在
```

> ⚠️ **易错点**：文件开头是 **hash**，第二个字符串才是 version。
> 早期按"前 16 字节读版本号"的做法全部得到 null。

语料实测：963 个 primary 单位中 `nonessential` 覆盖率、`imagesPath` 语义待 Phase 1 确认。

---

## 2. 对象映射

### 2.1 Skeleton / Bone

| Spine 3.2.01 字段 | Native Model | 语义与证据 |
|---|---|---|
| bone `name` | `Bone.name` | 展示用，可重命名 |
| bone `parentIndex` | `Bone.parentId` | **索引 → 稳定 ID 需 Phase 1 转换策略** |
| `x`/`y`/`rotation`/`scaleX`/`scaleY` | `Bone` 局部变换 | **Y 向下**（Spine 是 Y 向上；旧项目 IR 统一折成 Y 向上，新产品需明确规范方向，见 ADR-0003） |
| `length` | `Bone.length` | 世界空间长度 |
| `inheritRotation`/`inheritScale` | `Bone` 继承策略 | 布尔；Spine 3.6+ 用 transform 枚举表达，3.2 是两个布尔 |
| `color` | `Bone.color` | rgba |

**层级**：`parentIndex === -1` 表示根；实测第 0 根骨骼**没有** parent 字段
（`binary32.ts:113-115`），解析器按位置特判。这是私有分支与官方 2.1 的差异之一。

### 2.2 Slot

| 字段 | Native Model | 备注 |
|---|---|---|
| `name` | `Slot.name` | |
| `bone` | `Slot.boneId` | 索引 → ID |
| `color` | `Slot.color` | rgba |
| `blendMode` | `Slot.blend` | 枚举 |
| `attachment` | `Slot.defaultAttachment` | 默认皮肤下的附件名 |

Spine 的绘制顺序 = slot 数组顺序（可被动画 drawOrder 时间线改写）；
Spine **无 zIndex 概念**，Native Model 应以显式 `order` 字段承载（规范 §3.2）。

### 2.3 Constraint

语料实测（primary 963）：**IK 2,481 个、transform 160 个、path 0 个**。

| 类型 | 字段 | Native Model | 备注 |
|---|---|---|---|
| IK | `name`/`order`/`bones[]`/`target`/`mix`/`bendPositive` | `IkConstraint` | 首版必须支持（规范 §2.2） |
| Transform | `name`/`order`/`bones[]`/`target`/`mix*` | `TransformConstraint` | 后续阶段 |
| Path | — | `PathConstraint` | **语料 0 个**，无法用真实样本验证 |

> IK 在 Spine 里既出现在 constraints 段（2,481）也出现在动画时间线（36,408 条 ik 时间线）。
> Native Model 必须区分"约束定义"与"约束动画"。

### 2.4 Skin / Attachment

语料实测：**附件只有 `region`(24,605) 与 `mesh`(18,325，其中加权 15,751) 两类**；
`boundingbox`/`path`/`clipping`/`point` **0 个**。

| 类型 | 关键字段 | Native Model | 高风险语义 |
|---|---|---|---|
| region | `path`/`rotation`/`x`/`y`/`scaleX`/`scaleY`/`width`/`height`/`color` | `RegionAttachment` | `width/height` 是**绘制尺寸**（原始图尺寸），非图集像素尺寸；图集缩放由 `AtlasRegion` 承载 |
| mesh | `path`/`uvs`/`triangles`/`vertices`/`hull`/`color` | `MeshAttachment` | 非加权 mesh 的 `vertices` 是**槽骨骼局部坐标**；`uvs` 是 **regionUVs（region 内相对 [0,1]）**，整页归一化 UV 由 runtime `UpdateUVs()` 加载时计算 |
| weighted mesh | 同上 + `[boneCount, (boneIdx,x,y,w)×n]` 交错式 | `MeshAttachment` + weights | `boneIdx` 是**骨架全局**下标；`bones` 是该网格参与骨骼名表 |
| （语料缺失）boundingbox / path / clipping / point | — | — | **无真实样本**，只能合成 fixture |

**UV 语义（本次调查的核心结论，务必保持）**：
Spine 文件里 mesh 的 `uvs` 就是 regionUVs（region 内相对），
`UpdateUVs()`（`MeshAttachment.cs:98-114`）才由它算出整页归一化 UV。
任何"把 UV 当整页坐标再逆变换"的处理都会构成双重变换、把 UV 拉出 [0,1]。

### 2.5 AtlasRegion

| `.atlas` 字段 | Native Model | 语义 |
|---|---|---|
| `xy` | `AtlasRegion.x/y` | 页内左上角（**已打包、可能被旋转**） |
| `size` | `AtlasRegion.width/height` | **打包后**尺寸；`rotate:true` 时图集内实际占位是 `(h, w)` |
| `rotate` | `AtlasRegion.rotateDeg` | 实测只有 `true`/`false`（等价 90°/0°），无 `90`/`180`/`270` 字符串写法 |
| `orig` | `AtlasRegion.originalWidth/Height` | 未打包（未裁剪）尺寸 |
| `offset` | `AtlasRegion.offsetX/Y` | trim 偏移；**sdorica 语料实测 0% 裁剪**（offset 全 0、size==orig） |
| `index` | `AtlasRegion.sequenceIndex` | 序列帧索引，实测全 `-1`（无 sequence 附件） |

语料另有**图集打包缩放**：部分单位（如 b0001s3）原始尺寸 ÷ 图集尺寸 ≈ 1.667，
即图集被打包时统一缩小。缩放应放在图集层还是附件层，属 ADR 待决项
（Spine 用 `regionScale = width/regionOriginalWidth * scaleX`，DragonBones 用图集层 `scale` 取倒数）。

---

## 3. 动画与时间线

语料实测（primary 963）：动画 15,067 个、时间线 4,676,323 条，构成：

| 时间线类型 | 条数 | Native Model |
|---|---|---|
| 骨骼（rotate/translate/scale） | 3,423,557 | `BoneTimeline` |
| 插槽（attachment/color） | 698,760 | `SlotTimeline` |
| deform（FFD） | 316,722 | `DeformTimeline` |
| ik | 36,408 | `IkConstraintTimeline` |
| drawOrder | 14,136 | `DrawOrderTimeline` |
| event | **0** | `EventTimeline`（语料无法验证） |

**帧值语义（最高风险，旧项目踩过坑）**：
Spine 的骨骼动画帧值是**相对 setup 的增量**，不是绝对值。
旧项目第一版按绝对值减 setup，导致 mecha/idle/root 差 90°。
Native Model 必须在 schema 层写死这一点（规范 §3.2）。

**deform 语义**：Spine 对**非加权 mesh 是增量**、对**加权 mesh 是绝对值且按 `I.length/3*2` 展开**；
IR 只有"每顶点 2 个数的增量"。Native Model 需定义统一的形变表示与换算规则。

**时基**：Spine 二进制用**浮点秒**。规范 §3.2 要求动画时间用整数 tick/帧位置存储、
在 Project 定义帧率/时间基。**换算规则（有理数/舍入）需 Phase 1 依据样本决定**。

**曲线**：Spine 支持线性 / stepped / 贝塞尔（4 点）。二进制路径还可能出现
"曲线采样点"形态（旧项目记为 `raw`）。Native Model 至少覆盖前三种，采样点形态待评估。

---

## 4. 不确定项（Phase 1 必须解决）

| # | 不确定项 | 为什么重要 | 建议动作 |
|---|---|---|---|
| U1 | **坐标系规范方向**：Spine Y-up，旧项目 IR 折成 Y-up，DragonBones Y-down | 决定所有变换字段符号；猜错全盘皆错 | ADR-0003 定案，用 golden 数值锁定 |
| U2 | **时基**：整数 tick vs 浮点秒；帧率来源（Spine 头无 fps 字段，旧项目写死 30） | 决定动画精度与往返一致性 | 实测 sdorica 动画帧率假设，ADR-0003 |
| U3 | **`nonessential` 段语义**：`imagesPath`、mesh `edges`/`width`/`height`、事件字符串 | 影响哪些字段可安全丢弃 | 统计覆盖率后定"必需/可选" |
| U4 | **附件 `path` 与附件名不一致**的情况（3.x 允许） | 影响图集 region 查找键 | **✅ 已量化**：显式 path **84** 个，且 **84/84** 与附件名不同；查找键 `path ?? name` 命中 47,705 / 未命中 **4**（未命中根因为附件名尾随空格）。见 `docs/skeleton/field-mapping.md` §5.1、§10.1 |
| U5 | **boneIdx 全局下标 → 稳定 ID** 的迁移策略 | 影响权重数据可迁移性 | ADR-0004 |
| U6 | **图集缩放承载层**（图集层 vs 附件层） | 影响 region 附件尺寸正确性 | **🟡 承载层已定**：region 坐标空间 = 页头声明 `size`，PNG 为更高分辨率同一图，缩放 `pixel/page` 在**页层**应用（像素判据 26 支持 / 0 反对 / 3 不确定）。**rotation≠0 的复合仍待验证**（T-A1）。见 `docs/skeleton/skeleton-domain-rfc.md` §6.7 |
| U7 | **drawOrder 的表示**（偏移量表 vs 全序表） | 影响编辑与往返 | **提案：全序表**（`Skeleton.drawOrder` = slot ID 有序数组，与 `slots[]` 互为排列并冗余校验）。源无 zIndex，slot 数组顺序即绘制顺序；语料 drawOrder 时间线 14,989（Full）。见 `docs/skeleton/skeleton-domain-rfc.md` §5.1、§6.5 |
| U8 | **事件/音频事件**：语料 `ir.events` 仅 1 个、event 时间线 0 条 | 首版是否支持事件缺真实样本 | **已定：不支持且有诊断**（语料无样本可验证往返）。见 `docs/skeleton/field-mapping.md` §6 |
| U9 | **boundingbox / clipping / point / path 附件**：语料 0 个 | 同上 | 同上 |
| U10 | **path 约束**：语料 0 个 | 首版范围 | 能力矩阵标 `不支持` 或 `待验证` |
| U11 | **linked mesh**（`parent`/`skin`/`inheritDeform`） | 共享网格的导入/导出 | **✅ 已量化**：**2** 例（Full / primary 均为 2）→ schema 需要 `parentName` 字段，但无真实语料可做往返验证。见 `docs/skeleton/field-mapping.md` §5.2 |
| U12 | **外部资源定位**：`.atlas.txt` 的页文件名 → png 的解析规则 | 影响素材重连 | **🟡 已实测，需按新结论复核**：页头 `size` 与伴随 PNG 的 IHDR **1107/1111 不一致**（PNG 恒为 2 的幂，页声明为其内容尺寸）。**region 坐标属页声明空间**，不得把 PNG 像素尺寸当作 region 坐标空间。见 `docs/skeleton/field-mapping.md` §9、§10.2 |

> **证据修订**：2026-10-20（Task 4 阶段 A）。标 ✅ = 已量化；🟡 = 部分解决、仍有残留项。
> 全部证据由 `tools/skeleton-probe/` 只读扫描产出、可复跑；逐字段映射见
> [`docs/skeleton/field-mapping.md`](skeleton/field-mapping.md)，契约见
> [`docs/skeleton/skeleton-domain-rfc.md`](skeleton/skeleton-domain-rfc.md)。

---

## 5. 语料能力边界（供 Phase 1 能力矩阵直接引用）

| 能力 | 语料实证 | 结论 |
|---|---|---|
| bone / slot / region | 65,171 / 38,821 / 24,605 | 可完整验证 |
| mesh（非加权 + 加权） | 18,325 / 15,751 | 可完整验证 |
| deform（FFD） | 316,722 条时间线 | 可完整验证 |
| IK | 2,481 约束 + 36,408 时间线 | 可完整验证 |
| transform 约束 | 160 | 可验证（样本少） |
| drawOrder | 14,136 条 | 可验证 |
| event | 1 个 event、0 条时间线 | **几乎无法验证** |
| path 约束 | 0 | **无法验证** |
| boundingbox / clipping / point / path 附件 | 0 | **无法验证** |
| sequence 附件 | `index` 全 -1 | **无法验证** |
| 文件完整性 | strictEof 0 失败 / 963 | 无截断/损坏 |
| 版本 | 全部 `3.2.01` | 单一版本，无版本分叉 |
