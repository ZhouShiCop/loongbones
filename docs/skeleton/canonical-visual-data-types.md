# Canonical 视觉数据类型（Task 4 阶段 B 前置）

- 状态：**B0 冻结稿，复核通过**（2026-10-20；类型边界与扩展规则已冻结）。
  冻结后改动须走显式修订，不得由实现阶段顺手改语义。
- 目的：先确定图片、图集、几何与 UV 在 Loongbones 中分别代表什么，再将 Spine 源字段映射到这些类型。
- 约束：Spine/DragonBones 是可插拔导入源；本文件描述 Loongbones 自己的数据，不把源格式当内部交换格式。
- 配套：v2 schema 冻结见 `../format/native-project-v2-skeleton.md`；源字段映射见 `field-mapping.md`。
- 证据范围：本文件的语料证据全部来自 **Spine 3.2.01**；**尚未完成 DragonBones 对照**，
  相关结论不得表述为跨格式通用结论（详见 §7.0 证据范围声明）。

## 1. 类型分层

```text
PNG / 其它图像文件
  └── TextureAsset：实际像素资源及其存储位置/像素尺寸
        └── AtlasPage：图集坐标平面，引用一个 TextureAsset
              └── AtlasRegion：该页中的打包矩形 + 原图/裁切/旋转元数据
                    ├── Scene SpriteNode：场景中的普通图片节点，直接引用 AtlasRegion
                    └── Skeleton Attachment：骨架内部由 Slot/Skin 选用的可绘制内容
                          ├── RegionAttachment：用矩形绘制一个 AtlasRegion
                          └── MeshAttachment：用顶点/三角形/UV 绘制一个 AtlasRegion
```

这几层不能合并成一个“图片节点”：文件、图集切片、场景实例和骨架附件的身份及变换语义不同。

## 2. Loongbones 数据类型目录

| 类型 | 表示什么 | 关键字段/约束 | 已有基础 |
|---|---|---|---|
| `TextureAsset` | 一张实际图像文件的资源记录 | 存储模式、项目相对位置、像素宽高；不包含 atlas 切片 | `domain/atlas.hpp` |
| `Atlas` | 多个图集页与区域的逻辑容器 | 唯一 ID；持有 pages 与 regions | `domain/atlas.hpp` |
| `AtlasPage` | Atlas 坐标平面，并绑定一张 TextureAsset | `width/height` 是 atlas 声明的坐标单位；引用一个 texture | `domain/atlas.hpp` |
| `AtlasRegion` | 图集页中一个逻辑图像区域 | 打包矩形、rotation、original/trimmed 尺寸与 offset；尺寸方向遵守 `atlas.hpp` 契约 | `domain/atlas.hpp` |
| `Scene SpriteNode` | Scene 树里一个可编辑的图片实例 | 引用 `AtlasRegion` 或旧式 `ImageAsset`，带 Scene transform | `domain/node.hpp`；Task 2 |
| `RegionAttachment` | Skeleton 的 Slot/Skin 选中的矩形图片附件 | region 引用 + 骨骼局部位置/旋转/缩放 + 逻辑尺寸 + 颜色 | RFC 提案；未实现 |
| `MeshAttachment` | Skeleton 中用网格而非矩形绘制的附件 | region 引用 + 顶点 + 三角索引 + regionUVs | RFC 提案；本次垂直切片不实现 |
| `WeightedMesh` | Mesh 顶点由多个 Bone 按权重变形的网格 | 每顶点 0..N 个 `(boneId, weight, localPosition)` 影响；每顶点权重和必须为 1（容差规则另定） | 后续任务，不进入 Task 4 首片 |

## 3. UV 与几何语义

### 3.1 UV

- Mesh UV 定义为 **regionUV**：每个网格顶点的 UV 位于对应 `AtlasRegion` 内，通常在 `[0,1]`。
- UV 不保存整张 PNG 的像素坐标，也不保存 atlas-page 像素坐标。
- Region 的 rotation、packed rectangle 和 page→PNG 像素缩放由后续渲染/纹理映射阶段统一组合；导入器只保存源语义，禁止先转成整页 UV 后又在渲染时重复变换。
- Spine 源数据 `uvs` 的语料证据和规则见 `skeleton-domain-rfc.md` §6.10 / `DECISIONS.md` N3。

### 3.2 网格几何

- `vertices` 是有序顶点列表；`triangles` 是按三角形展开的顶点索引列表，长度必须为 3 的倍数且每个索引在顶点范围内。
- 非加权 Mesh 的顶点空间必须按 Spine 3.2.01 的源语义映射，不能仅因名称叫 local/world 而推断；语料支持证据见 `DECISIONS.md` N2，阶段 B 以 golden fixture 数值固定。
- 加权 Mesh 的每个顶点拥有独立 bone influence 列表；不得将它展平成单个骨骼索引，也不得把权重数据写入 RegionAttachment。
- Mesh/WeightedMesh 能力后续通过独立模型字段和 required capability 扩展；旧 Reader 遇未知必需能力必须拒绝，不能静默当作 RegionAttachment。

## 4. 层级对象与绘制关系

```text
Skeleton
  ├── Bone[]：骨骼层级和 setup pose
  ├── Slot[]：绑定一个 Bone；含默认显示/颜色/blend
  ├── drawOrder[]：Slot 的显示顺序
  └── Skin[]
       └── SkinEntry[]： (slotId, attachmentName) → Attachment
             ├── RegionAttachment → AtlasRegion
             └── MeshAttachment → AtlasRegion + geometry
```

Bone 是 Skeleton 内部变换载体，不是 SceneNode。Slot 是绘制槽，不是 AtlasRegion。SkinEntry 负责
“某个皮肤中某 Slot 的某附件名对应哪种绘制内容”。

## 5. 首片实现范围

阶段 B 第一个可运行切片只导入 **`.atlas.txt` + 页头引用的 PNG**，产出 Loongbones `TextureAsset`、
`AtlasPage`、`AtlasRegion` 并保存到原生工程。它：

- 不读取 `.skel.bytes`；
- 不创建 Skeleton、Bone、Slot、Skin 或 Attachment；
- 不读取或解码 Spine 动画/网格数据；
- 验证页坐标、PNG 像素尺寸、rotation、trim、资源相对路径和保存/重开流程。

该切片通过后，再在同一插件宿主中加入 Skeleton / Bone / Slot / Skin / RegionAttachment 映射。
Mesh/WeightedMesh、约束和动画仍属后续任务。

## 6. 已冻结项（B0，2026-10-20）

| # | 项 | 冻结结论 | 依据 |
|---|---|---|---|
| 1 | v2 中可重排且可携带未知字段的对象必须有稳定宿主 ID | **`SkinEntry` 增加 `id`（ULID）**，并新增 `OwnerKind::kSkinEntry`；锚点 = `(kSkinEntry, entryId)`。`Skeleton`/`Bone`/`Slot`/`Skin` 同样新增各自 `OwnerKind`。`Attachment` **不单独给 ID**：与 SkinEntry 1:1 内嵌，锚点用 `(kSkinEntry, entryId) + /attachment`。**不得**以数组下标或自然键作锚点 | v2 schema §5；D22 |
| 2 | `TextureAsset` 首片默认策略 | **复制进工程**：`storage = "inProject"`，`location = assets/textures/<TextureAssetId>.png`（工程相对路径）。源 PNG 只经 `IResourceResolver` 只读解析，由宿主资源暂存服务写盘；插件无任意路径写权限。`external` 保留给用户显式创建的外部引用，导入器永不产出 | v2 schema §7；B0 计划项 7 |
| 3 | 页坐标到 PNG 像素的缩放 | **只由页层承担**：`AtlasPage.width/height` = 图集坐标空间，`TextureAsset.pixelWidth/Height` = 实际像素，缩放 = `pixel/page`，渲染时于页层应用一次。Region 坐标不预先缩放 | v1 已发布字段已够用；语料实证见 `skeleton-domain-rfc.md` §6.7 |
| 4 | `RegionAttachment` 与 Scene `SpriteNode` | **不同类型**：可引用同一 `AtlasRegion`，但变换父级（骨骼 vs 场景节点）、持有者（SkinEntry vs Scene）与保存位置不同。Task 4 不创建 Scene 实例（D27） | 本文件 §1/§2 |
| 5 | `Skin` 所有权 | **只嵌套于 `Skeleton`**；Project 顶层无 `skins[]` | `skeleton-domain-rfc.md` §3.1 |
| 6 | 两种 hash | `sourceRef.sourceHeaderHash`（源自带头字段原文，可选）与 `sourceRef.contentHash`（宿主对 `.skel.bytes` 全文算的 SHA-256）**分立**，不得混为一字段 | `skeleton-domain-rfc.md` §8.3 |
| 7 | 诊断等级 | error（结构损坏/非法引用/缺纹理/非有限浮点/未知附件类型/不支持的源版本）拒绝整个候选；warning（不支持能力、尾随空格降级）需用户明确确认后才提交；未进入切片的数据只产统计与诊断，不建半成品 Domain 对象 | v2 schema §9 |
| 8 | 能力与版本 | 每个 v2 文档声明 `loongbones.skeleton.v1` 为 required；四类版本/能力错误码各自单一固定 | v2 schema §6；D24 |

### 6.1 仍未冻结（明确留给后续任务）

| 项 | 状态 |
|---|---|
| Mesh / WeightedMesh 的逐字段数值映射 | 待 Spine 3.2.01 golden 证据，另行纳入任务 |
| 加权顶点的权重和归一化容差 | 待定（§7.1 案例） |
| `BindingBase` / `AutoBind` 是否有必要 | 待合成几何与样本验证（§7.1 案例） |
| 约束、变形、动画曲线的类型层次 | 待各功能独立分析 |

## 7. 按功能建模，而不是照抄源格式

Spine 与 DragonBones 是功能和数据语义的研究对象，不是 Loongbones 类型表的模板。每项能力应先分别记录各源格式的表现方式、运行时效果、限制和数据证据，再确定 Loongbones 的功能契约与类型层次。若两种源格式的做法不足以支持 Loongbones 的编辑/演进需求，可以新增自有抽象；导入器负责把已验证的源语义转换到该抽象。

**设计取舍：先实现基础，但让基础能稳健扩展。** 当前只落地已验证且首期必需的最小类型；同时从第一版就定义稳定 ID、对象所有权与引用规则、未知字段宿主、能力/版本扩展边界和默认值语义。扩展应通过新增可选对象/字段或显式 capability 完成，旧数据不因新增能力而改写含义；避免为尚未验证的功能预造大量空泛基类、万能字典或仅为某个源格式服务的字段。每个扩展都应有独立语义、迁移/读取规则和兼容测试。

建议每项能力形成一张对照表：

| 项 | 需要回答的问题 |
|---|---|
| 功能目标 | 用户/运行时希望实现什么可观察行为？ |
| 源格式证据 | Spine 与 DragonBones 分别存了什么字段、引用和数值？证据来自哪些原始样本/合成 fixture？ |
| 语义解释 | 这些数据在编辑、求值、变形或渲染时如何起作用？哪些是格式事实、哪些仍是推断？ |
| Loongbones 契约 | 本项目要承诺什么行为、不承诺什么行为？ |
| 类型分层 | 哪些是稳定定义/配置，哪些是逐顶点或逐对象的值，哪些只是生成操作/算法？ |
| 转换与降级 | 可无损映射、需转换、需用户确认，还是明确不支持？ |
| 演进与验证 | 类型 ID、向下兼容、版本化参数、边界测试和 golden fixture 如何处理？ |

### 7.0 首批功能的对照表（B0 冻结）

> **证据范围**：以下四张表的「源格式证据」行**全部来自 Spine 3.2.01 语料**，
> **尚未完成 DragonBones 数据对照**。因此这些结论目前只对 Spine 导入路径成立，
> **不得**表述为跨格式通用结论；也不得据此推断 DragonBones 需要或不需要同样的分层。
> DragonBones 的研究与导入须独立立项，完成后应回头复核本节的「Loongbones 契约」与「类型分层」两行
> —— 那两行描述的是 Loongbones 自己的承诺，理论上与源格式无关，但必须经跨格式证据确认后才算验证过。
>
> 这不阻塞 Spine 首片（B1）；它只是划定当前结论的适用范围。

#### 7.0.1 纹理资产存储与位置（TextureAsset）

| 项 | 结论 |
|---|---|
| 功能目标 | 工程可携带、可迁移；关闭源目录后纹理仍可解析 |
| 源格式证据 | `.atlas.txt` 页头 `file` 给出 PNG 文件名（相对 atlas 所在目录）；语料 1111 页全部单页，PNG 与 `.atlas.txt` 同目录（`sample-manifest.md` §1b） |
| 语义解释 | 页头 `file` 是**源内相对引用**，不是工程内路径；源目录只读且可能被移除 |
| Loongbones 契约 | `TextureAsset.storage = "inProject"`，`location = assets/textures/<TextureAssetId>.png`；工程相对路径、POSIX 分隔符、禁 `..`/绝对路径。导入器**永不**产出 `external` |
| 类型分层 | 数据：`TextureAsset`（资源记录）。操作：宿主的资源暂存/复制/回滚。二者分离 |
| 转换与降级 | 源 PNG → 工程内副本（字节复制 + hash/尺寸校验）。缺失源 PNG → **error**，拒绝候选 |
| 演进与验证 | 新增 `external` 的导入产出须显式选项 + 单独能力；回归覆盖路径形状校验、缺失纹理、写失败回滚 |

#### 7.0.2 图集页层缩放（AtlasPage ↔ TextureAsset）

| 项 | 结论 |
|---|---|
| 功能目标 | 同一 region 坐标在页声明空间与 PNG 像素空间之间无损对应 |
| 源格式证据 | 页头 `size` 与伴随 PNG IHDR **1107/1111 不一致**（PNG 恒为 2 的幂且更大）；像素判据 26 支持 / 0 反对 / 3 不确定（`skeleton-domain-rfc.md` §6.7） |
| 语义解释 | region 坐标属**页声明空间**；PNG 是更高分辨率的同一张图；缩放 = `pixelWidth / page.width` |
| Loongbones 契约 | `AtlasPage.width/height` = 页声明坐标；`TextureAsset.pixelWidth/Height` = 实际像素；比值**只在页层**应用一次 |
| 类型分层 | 数据：页尺寸 + 纹理像素尺寸（两个独立字段）。操作：渲染时的页层缩放 |
| 转换与降级 | 不缩放 region 坐标（缩放写进 region 会造成双重变换） |
| 演进与验证 | golden 必测页声明尺寸 ≠ PNG 尺寸的样本；T-A1（rotation≠0 与缩放复合）待验证 |

#### 7.0.3 骨架矩形附件（RegionAttachment）

| 项 | 结论 |
|---|---|
| 功能目标 | 用一个 `AtlasRegion` 在骨骼局部空间绘制一张图 |
| 源格式证据 | region 附件 27,459 个（Full）；`rotation` 非零 24,351、`scale ≠ 1` 744；`path` 显式 84 且 84/84 与附件名不同；`path ?? name` 命中 47,705 / 未命中 4（尾随空格） |
| 语义解释 | 附件持有骨骼局部的 x/y/rotation/scale 与逻辑 width/height；region 查找键是 `path ?? name` |
| Loongbones 契约 | `RegionAttachment { type:"region", regionId, x, y, rotationDeg, scaleX, scaleY, width, height, color }`；`regionId` 必须指向已存在 `AtlasRegion` |
| 类型分层 | 数据：附件（SkinEntry 内嵌值对象）。无操作层 |
| 转换与降级 | `path ?? name` 精确匹配优先；未命中 → error `SKELETON_ATTACHMENT_REGION_UNRESOLVED`；尾随空格降级为 warning（T-B3） |
| 演进与验证 | golden 覆盖 `path ≠ name`、rotation≠0、scale≠1、未命中与尾随空格；合成 fixture `02-nested-slots-regions` |

#### 7.0.4 皮肤条目宿主与 ID（SkinEntry）

| 项 | 结论 |
|---|---|
| 功能目标 | 未知字段在条目被重排/重命名后仍归属正确宿主 |
| 源格式证据 | skin 段为 `slotIndex + attCount + [skinKey, attachmentName, type…]` 的**数组**；源无稳定 ID，只有下标与名称 |
| 语义解释 | 条目是可重排数组；名称（附件名）可被用户编辑 |
| Loongbones 契约 | `SkinEntry.id`（ULID）+ `OwnerKind::kSkinEntry`；锚点与位置解耦。同 Skin 内 `(slotId, attachmentName)` 唯一 |
| 类型分层 | 数据：entry ID（稳定身份）。操作：无 |
| 转换与降级 | 导入时由宿主 `IIdFactory` 分配；源下标/名称只作单次导入内的映射键 |
| 演进与验证 | 回归覆盖重排/重命名/删除宿主后未知字段跟随或丢弃；v2 schema §5.3 记录被否决的替代方案 |

### 7.1 绑定/蒙皮作为待分析的类型分层案例

“绑定”先作为功能名称，不预设等同于某一个源格式对象。至少区分以下概念：

1. **绑定目标与范围**：哪个几何/附件由哪个 Skeleton 的哪些 Bone 影响；引用必须使用稳定 ID。
2. **绑定定义/基础（候选 `BindingBase`）**：稳定的绑定关系、坐标空间及规则配置。只有在多个绑定实例确实共享此类定义，或需独立编辑/复用时，才引入此层。
3. **绑定值（候选 `BindingValue` / `BindingInfluence`）**：顶点到 Bone 的影响及权重；每顶点影响数、骨骼引用、权重有限性/范围/归一化规则须由目标行为与源样本证据确定。
4. **自动绑定（候选 AutoBind）**：优先视为“生成/重算绑定值的命令或算法”，而不是和手工权重混在一起的运行时绑定数据。若用户需要可重复编辑，则可持久化算法标识、版本、输入参数和可选生成来源；最终生效值仍应有明确表示，不能依赖某个版本算法临时重算才可正确打开工程。

以上名称与层次均为**设计候选，不是已冻结 API/schema**。分析时应分别考察 Spine weighted mesh、DragonBones 对应蒙皮/权重数据及其运行语义，使用合成几何验证顶点数、骨骼引用、权重和坐标空间，再决定 `BindingBase` 是否必要、AutoBind 元数据是否持久化，以及权重值如何组织。当前 Task 4 静态首批不实现绑定、Mesh 或权重；这些应在骨架静态附件闭环之后，以独立功能规格和兼容性审阅推进。

类似方法也适用于约束、变形、插槽显示、动画曲线等：先定义功能语义与本项目编辑需求，再决定数据类型，不因某个源格式恰好使用单一对象就限制 Loongbones 只能采用同样层数。

