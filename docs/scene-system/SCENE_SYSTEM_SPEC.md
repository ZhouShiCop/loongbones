# 场景系统基础模型（Scene System Foundation）

- 状态：设计基线（已按负责人确认的可挂载规则更新）；Task 2 实施以本文件为准
- 项目：`D:\Works\Spine\Loongbones`
- 目标：定义最小场景树、节点、Sprite 与 Atlas Region 的职责边界；不在本文中直接定下具体 C++ 类实现或文件格式字段编号

## 1. 设计结论

场景由一个必需的 `RootNode` 和其下的节点树组成。普通节点共享局部变换与层级属性；无渲染内容的容器用 `EmptyNode`；可绘制图片用 `SpriteNode`。图集及图集区域是资源数据，不是节点类型。`SpriteNode` 引用 `AtlasRegion`，区域再关联到图集页与纹理资源。

```text
Scene
└── RootNode                 场景唯一根，永远存在，不直接绘制
    ├── EmptyNode             空容器，可组织子节点
    │   └── SpriteNode        图片实例，也可以组织子节点
    │       └── SpriteNode
    └── SpriteNode

SpriteNode → AtlasRegion（资源引用，不属于场景树）→ AtlasPage → TextureAsset
```

## 2. 基础类型及职责

| 类型 | 建议形态 | 职责与约束 |
|---|---|---|
| `Scene` | 持有型数据对象 | 场景 ID、名称、唯一根节点 ID、场景设置；初期可每项目默认一个 Scene，但不要把 Project 与 Scene 混成同一类型。 |
| `Node` | 抽象基类或等价公共数据 | 稳定 ID、名称、父节点 ID、显式有序子节点 ID、局部 Transform、可见性、锁定状态。不得保存 Qt 对象、纹理句柄或绝对路径。 |
| `RootNode` | 唯一根节点实现 | 无父节点；每个 Scene 恰有一个；不可删除/移动；固定在场景原点且使用恒等变换；不直接绘制；负责挂接顶层节点。 |
| `EmptyNode` | 普通节点实现 | 无自身绘制内容；可拥有子节点；用于分组、层级组织、父级变换与编辑器 Outliner。可作为新建节点的基础默认类型。 |
| `SpriteNode` | 图片渲染节点实现 | 关联一个 `AtlasRegion`；带实例级变换、可见性、色调/透明度（若首期需要）；可以拥有子节点，子节点继承它的世界变换。 |
| `Atlas` | 资源数据对象 | 图集逻辑身份及图集页列表；同一图集可有一页或多页。 |
| `AtlasPage` | 资源数据对象 | 引用实际纹理资源，并记录页面尺寸/采样元数据；不属于节点树。 |
| `TextureAsset` | 资源数据对象 | 图片资源逻辑身份与存储引用；资源可被多个 AtlasPage 使用，节点不复制纹理元数据。 |
| `AtlasRegion` | 区域记录 | 名称、所属页、打包矩形、旋转状态、原始尺寸、裁切尺寸/偏移；Sprite 引用它，不创建一个 RegionNode。 |
| `Transform2D` | 值对象 | 局部位置、旋转、缩放、轴心点；统一角度/坐标/组合顺序。Skew 是否纳入首版由明确需求和格式契约决定。 |

`Node` 是类型体系的公共基类；`RootNode`、`EmptyNode`、`SpriteNode` 是具体节点类型。不要把“Root”当成所有节点的抽象类型，也不要同时建立两个职责相同的泛化根类。

## 3. Transform、尺寸与绘制顺序

### 3.1 Transform

- 每个非根节点保存**局部**位置、旋转、缩放和轴心点；世界变换由父链求值，不持久化冗余世界矩阵。RootNode 固定在场景原点，局部变换为恒等值。
- 明确坐标系（当前总体 ADR 采用 Y-up）、角度单位、矩阵布局和组合顺序；与 `docs/adr/0003-units-and-coordinate-contract.md` 对齐。
- 初始建议字段：`position(x,y)`、`rotationDeg`、`scale(x,y)`、`pivot(x,y)`。缩放默认 `(1,1)`，其余默认 `(0,0)` / `0`。
- 零缩放是否允许、负缩放镜像语义及非有限数值处理必须有契约和测试；不由 Qt 控件各自决定。

### 3.2 尺寸

不建议把 `width/height` 当成所有 Node 的必需公共属性：EmptyNode 没有固有绘制尺寸。SpriteNode 的自然尺寸由 Region 的原始图像尺寸决定；若支持拉伸，SpriteNode 再显式记录可选尺寸覆盖或绘制尺寸模式。裁切图集的显示 quad 必须结合原始尺寸和 trim offset 还原，不能只拿 packedRect 宽高作为角色显示尺寸。

### 3.3 子节点顺序

- 子节点数组顺序是稳定语义，明确作为同一父节点内的绘制顺序；不依赖 map/hash 顺序。
- SpriteNode 可先绘制自身，再按有序子树绘制后代；若渲染器采用其他顺序，必须统一写入绘制契约，不能由节点类型隐式改变。
- 根节点的子节点也参与场景绘制顺序。
- 后续如要支持跨父级排序或独立 Z 值，再单独定义 `zIndex` 及 tie-break 规则，不以两个排序系统暗中叠加。

## 4. Atlas Region 与 Sprite 的边界

`AtlasRegion` 是被引用的区域资源，不是 Node。最小字段建议：

- 稳定 `regionId`、名称、`atlasPageId`；
- packed rectangle（x/y/width/height，像素单位）；
- 是否顺时针旋转/旋转枚举；
- 原始图像尺寸；
- 裁切后尺寸及相对原图的 trim offset；
- 未来扩展字段命名空间。

`SpriteNode` 保存 `regionId` 和实例属性；不能把区域像素坐标复制到每个 Sprite。UV 的 V 方向、旋转映射、trim 还原和过滤边界在渲染/导入契约中统一定义，并以不对称真实图案样本验证。AtlasRegion 删除/重命名应通过稳定 ID 引用处理；被 Sprite 引用时，删除要拒绝或要求显式替换/解除引用。

## 5. 场景树不变量

1. 每个 Scene 恰有一个 RootNode；RootNode 没有父节点、处于场景树顶层、不可移动/删除，且固定在场景原点使用恒等变换。
2. 除 RootNode 外，每个节点恰有一个有效父节点；父 `children` 与子 `parentId` 双向一致。
3. 不允许环、悬空引用、重复 ID、重复子引用或同一节点被多个父节点引用。
4. 普通节点不按渲染类型限制父子关系：EmptyNode、SpriteNode 均可作为父节点，也可拥有有序子节点；SpriteNode 自身绘制与子树绘制顺序按 §3.3 统一处理。唯一禁止的挂载对象是 RootNode（它只能是 Scene 根）。
5. 所有节点 ID 不含语义、创建后不变；名称变化不影响 ID。
6. Region、AtlasPage、TextureAsset 通过稳定 ID 关联；场景节点不拥有/复制资源数据。
7. 任何写操作先校验完整前置条件再提交；失败不得改变节点树、资源引用或顺序。
8. 删除 RootNode 永远拒绝；删除有子节点的普通节点必须显式选择拒绝、提升子级或删子树之一；不得静默丢弃后代。
9. 场景层级一致性校验验证根节点唯一性、节点类型与载荷一致性及父子关系；普通节点类型不得被用作“不可挂载”的限制。
10. 加载/导入时未知节点类型或未知扩展必须保留原始数据或以明确诊断拒绝，不可静默丢弃再保存。

## 6. 与 Task 1 现有模型的映射

| Task 1 当前概念 | 场景模型目标 | 处理建议 |
|---|---|---|
| `Project` | `Project` | 继续作为工程级容器；以后包含一个或多个 `Scene`。 |
| Root group node | `RootNode` | 从普通可操作分组中区分出来，固定根不变量。 |
| `GroupNode` | `EmptyNode` | 名称可沿用或迁移，但术语统一；它是普通可编辑容器。 |
| `ImageNode` | `SpriteNode` | 只保存实例级状态和区域引用，不内嵌贴图。 |
| `ImageAsset` | `TextureAsset` / `AtlasPage` | 需要区分原图资源、图集页纹理和 Region 区域；不要将这三个身份混为一类。 |
| 单一图片引用 | `AtlasRegion` 引用 | Task 1 目前还没有 Atlas/Region 数据；作为后续独立 Task 加入，不在现有基础模型中偷偷增加混合字段。 |
| `Transform` | `Transform2D` | 与坐标 ADR 对齐；决定 pivot/size 覆盖后再固化序列化字段。 |

Task 1 的 Back_task 曾按“只有 GroupNode 可容纳子节点”修复；该限制已由负责人澄清为不符合目标场景规则。Task 2 应保留 GroupNode 的既有类型和 API，同时允许所有普通节点（含 ImageNode/SpriteNode）挂载子节点；只有 RootNode 受唯一根、固定位置及不可移动/删除约束。不得通过改名或重建 ID 破坏 Task 1 数据身份。

GroupNode（后续统一术语可称 EmptyNode）与 SpriteNode 都是 Node 的具体类型。GroupNode 的差异在于自身不绘制内容，可用于组织层级和承载父级变换；它不享有独占子节点的权限。折叠、批量选择等纯编辑器便利能力不写入核心场景模型，除非另有明确需求。

## 7. 推荐实现顺序

### S1：场景树基础契约

定下 `Scene`、唯一 `RootNode`、普通 `Node`、`EmptyNode`、`SpriteNode` 的角色；稳定 ID、Transform 和顺序；实现所有普通节点通用的挂载规则；为每项树不变量添加测试。

### S2：纹理与图集资源模型

区分 `TextureAsset`、`AtlasPage`、`Atlas`、`AtlasRegion`；明确外部资源引用、路径安全、hash/去重策略、多个图集页、region 删除与重连策略。

### S3：Sprite 几何与预览

定义 Region → Sprite 本地 quad 的尺寸、pivot、trim、旋转和 UV 映射；先完成离屏/确定性数值校验，再接 Qt 渲染视口。

### S4：Scene 文件序列化与 GUI

将场景结构纳入原生格式 schema 和迁移规则；GUI 提供 Outliner、Viewport、属性面板和资源浏览器；操作全部经应用命令并支持撤销/重做。

## 8. 暂不纳入基础模型

Camera、文字、粒子、TileMap、网格/蒙皮、动画/约束、物理、脚本、Prefab/继承场景、跨场景实例、多线程渲染、复杂材质与插件系统暂不进入本基础 Task。若其中任何一项成为近期必需能力，再按独立 RFC 增加类型或组件，并给出迁移与兼容策略。

## 9. 验收标准

- 可构造一个合法 Scene：唯一 RootNode、若干 EmptyNode 与 SpriteNode。
- 所有非法父子关系、环、孤儿、重复引用均被写操作拒绝并被整体校验报告。
- 写操作失败后数据快照一致；Root/删除策略按显式规则工作。
- Region/Atlas/Texture 与场景节点职责分离，Sprite 仅通过稳定 ID 引用区域。
- 坐标、旋转、尺寸、pivot、trim、绘制顺序与 UV 的单位/顺序均有可测试的书面契约。
- Qt、JSON codec 和渲染后端没有侵入纯 Domain 场景树类型。
- 文档对应的实施任务与 `STRUCTURE.md` 已登记；所有验证工具继续遵守项目规范中的就近说明和根结构文件登记要求。
