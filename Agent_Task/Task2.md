# TASK 2：场景领域模型与通用节点层级

- 状态：`COMPLETED`（Back Task 两项返修已复核；现有 build_t5 测试 67/67 通过，详见 `Agent_Task/Task2_Report.md`）
- 依赖：Task 1 Core Asset Model；`docs/scene-system/SCENE_SYSTEM_SPEC.md`；`项目规范.md`；ADR-0002/0003
- 执行范围：只实现 Qt 无关的 Scene/Node/资源领域模型与回归测试；本文件是可交给本地 Agent 的执行规格
- 不包含：JSON 编解码、Spine 导入、渲染、Qt GUI、动画/约束、DragonBones 导出

## 1. 目标

在 Task 1 的 Project、稳定 ID、资源引用与原子操作基础上，建立场景树的明确领域契约，并使代码符合负责人确认的规则：**Root 唯一且固定；所有普通节点都可作为父节点和子节点。** GroupNode 与 SpriteNode 的职责不同，但 GroupNode 不独占挂载能力。

本 Task 只交付数据模型/API、校验、诊断和自动化测试，不实现序列化或 GUI。不要把“Spine 兼容”扩大到本 Task；Sdorica 3.2.0.1 语料仅可作为后续场景/资源映射的只读参考。

## 2. 术语与核心语义

为避免 `Root` 同时指抽象父类和场景树唯一根，按以下语义实现并在 API 文档中固定：

- `Node`：普通节点的公共抽象/公共数据契约，持有稳定 ID、名称、父 ID、有序子 ID、局部变换、可见性和锁定状态（锁定状态若此次加入须有明确默认值）。
- `SceneRoot`（或等价明确命名）：每个 Scene 唯一的树根节点；无父节点，固定在原点并使用恒等变换，不绘制、不可移动或删除。若沿用现有 `Project::rootNodeId` / `GroupNode` 表示，必须通过明确的兼容包装或类型约束表达此特殊身份，不能让任意 GroupNode 变成根。
- `GroupNode`：无自身绘制内容的普通节点，用于层级组织、父级变换和 Outliner 表达；可以拥有任意普通节点。
- `SpriteNode`：有自身图片内容引用的普通节点；可以拥有任意普通节点。自身绘制与其后代的顺序统一按有序 children 契约解释。
- `Project` 与 `Scene` 不混为同一概念。Task 2 至少支持现有工程中的一个默认 Scene；多 Scene 的公开操作/API 若不能在本 Task 内稳定定案，则明确留给后续 RFC，不偷偷塞入隐式全局状态。
- `AtlasRegion` 是资源记录，不是 Node；SpriteNode 以稳定 ID 引用资源区域。若把 Atlas/AtlasPage/TextureAsset 模型纳入本 Task，必须保持资源身份与节点实例分离；不得复制区域像素元数据到每个 SpriteNode。

**层级不变量**：Root 是唯一特殊挂载约束。任何普通节点（GroupNode、SpriteNode 及以后增加的普通节点）均可作为父节点和子节点；不得因节点的渲染类型拒绝父子关系。

GroupNode 的“新东西/用处”在本 Task 只定义为无自身绘制、组织层级和承载父级变换。折叠、批量选择、颜色标签等纯 GUI 功能不进入持久化领域模型，除非有明确需求和单独契约。

## 3. 允许修改范围

- `domain/`：扩展现有领域模型/API；保持 Qt、JSON 库、具体文件系统和渲染后端无依赖。
- `tests/`：更新过期的“ImageNode 不可有子节点”用例，并新增场景模型与层级回归测试。
- `docs/`：更新场景模型/API/数据约束说明、ADR 或决策台账（仅记录本 Task 实际定案）。
- `tools/`：仅在确有验证工具新增/变更时修改；每个工具必须有就近 README 并登记到根目录 `STRUCTURE.md`。
- 根目录 `STRUCTURE.md`、`DECISIONS.md`：同步更新 Task 状态、目录职责、工具登记及本 Task 形成的决策。

不得修改原始 Sdorica 数据；不得复制受限样本或 Runtime 源码进仓库。真实样本只能通过外部路径只读访问；合成 fixture 必须标明为合成数据。

## 4. 必须实现

### 4.1 Scene 与 Root

1. 表达 Scene 身份、名称和唯一根节点身份；Project 对 Scene 的所有权明确。
2. 根始终唯一、无父节点、保持恒等局部变换；禁止将根作为普通子节点、移动、删除或赋予第二个根。
3. 对 Task 1 现有 `Project::create()`、`rootNodeId()` 等调用提供向后兼容行为或清晰的兼容迁移方案；不能批量重建/变更既有 ID。

### 4.2 普通节点与挂载

1. GroupNode 与 ImageNode（如命名保留）/SpriteNode 都允许持有有序 children；创建、移动、重排、删除、adopt/import 校验遵循同一规则。
2. 移除“父节点必须是 GroupNode”“ImageNode 不能持有子节点”的限制。
3. 拒绝根被挂载、环、孤儿、重复子引用、多父引用、父子双向不一致、缺失节点及非法变换；校验位置须可定位。
4. 创建/移动/重排/删除等失败操作必须保持 Project/Scene 的完整快照不变；删除非空节点仍需显式策略，不静默丢弃后代。
5. 定义并测试 SpriteNode 自身绘制顺序与子树的顺序语义（建议先绘制自身，再按 children 顺序遍历），不得依赖哈希容器迭代顺序。

### 4.3 图集资源引用与变换

1. 实现纯数据模型 `TextureAsset`、`Atlas`、`AtlasPage`、`AtlasRegion`；资源记录与场景节点实例分离，一个区域可被多个 SpriteNode 引用。
2. `AtlasPage` 引用 TextureAsset；`AtlasRegion` 至少保存稳定 ID、名称、page ID、打包矩形、旋转状态、原始尺寸、裁切尺寸与 trim offset。所有尺寸/偏移单位统一为像素，明确矩形边界、旋转枚举和无裁切默认值。
3. SpriteNode 引用 `AtlasRegion` 稳定 ID；节点不复制 region 几何元数据。沿用 ImageAsset/ImageNode 的已有 ID/API；若需要适配为整图 Region，采用明确的兼容映射，不批量重建 ID。
4. 校验缺失引用、重复 ID、非正/越界矩形、无效旋转和非法尺寸；引用中的 Region/Page/TextureAsset 不允许被静默删除。删除策略需与 Task 1 资源引用保护一致。
5. 本 Task 不解析 Spine `.atlas` 或图片文件，不计算渲染 quad/UV，不解码图像；只负责领域数据、关联、校验和确定性值语义。
6. 复用 ADR-0003 已定案的 Y-up、角度单位、2×3 仿射行主矩阵、`world = parent × local`；不在 Task 2 另造坐标约定。
7. 如果现有 `Transform` 缺少 pivot/锁定等字段，不得为“将来可能需要”无条件扩字段。先在文档中说明是否属于本 Task 必需字段；新增持久化语义需给默认值和兼容说明。

### 4.4 数据/API 兼容

1. 保持 Task 1 已有稳定 ID 不变，节点重命名、变父、重排不改变 ID。
2. 对现有公共 API 优先采用向后兼容扩展；如确需移除/改名，必须给出兼容包装、弃用窗口及迁移说明，不允许静默破坏调用方。
3. 已发布的诊断枚举/字符串不可复用或更改含义。`kImageCannotHoldChildren` / `IMAGE_CANNOT_HOLD_CHILDREN` 曾用于现已废止的限制：保留其枚举位置和字符串，不得再用于合法节点挂载失败；可标记 deprecated，并为真正错误使用正确的新码。
4. 本 Task 尚不实现 JSON codec，因此不得声称完成磁盘格式迁移；记录未来 native schema 所需的 Scene/Root/Node 类型字段及默认规则，交由格式 Task 实现。

## 5. 测试与验收

至少覆盖：

- 空工程包含且只包含一个合法固定 Root；根不能移动/删除/被挂载。
- Group → Group、Group → Sprite、Sprite → Group、Sprite → Sprite 的合法嵌套。
- 在 Sprite 下创建/移动子节点成功；reorderChild 对任意普通父节点都能正确排序。
- Sprite 自身和后代的遍历/绘制顺序确定。
- Atlas/AtlasPage/TextureAsset/AtlasRegion 引用闭合；多个 Sprite 共享同一区域且节点变换独立；非法 Region 元数据可定位拒绝。
- 环、孤儿、重复引用、多父、父子不一致、无效资源和非有限变换被定位诊断。
- adoptData/import 型入口拒绝非法候选数据且不改变接收方快照。
- 所有失败写操作保持完整结构快照不变；删除策略显式且无子树丢失。
- Task 1 原有 API 行为、ID 稳定性、资源共享与删除保护回归通过。
- 旧 `test_parent_type.cpp` 中“不允许 Sprite/Image 作父节点”的断言必须改为允许路径；同时保留其原子性思想，补充真实失败条件的失败不变性断言。

验收命令使用登记的 `tools/core-tests/README.md` 构建与运行入口；从干净构建目录重新配置、编译并运行 CTest 与用例程序。报告总用例数、通过/失败/跳过、退出码、编译警告，以及工具链版本。不得只报告旧二进制结果。

验收通过条件：API/文档/测试三者对普通节点挂载规则一致；Task 1 已有 ID 与接口未遭破坏；核心模块仍 Qt 无关；所有自动化测试通过；结构/工具文档已同步；所有延期能力明确记录。

## 6. 不在本 Task 范围

- Qt Widgets、Blender 式工作区、Outliner/Viewport/属性面板。
- OpenGL/其他渲染后端、图像解码、GPU 资源管理。
- JSON 原生格式、版本迁移、自动保存、打包和内部 codec。
- Spine `.atlas` 文件读取及 Spine 3.2.0.1 二进制导入。
- Spine 3.2.0.1 二进制完整解析/导入、动画、骨骼、蒙皮、网格、约束。
- 重新生成 Task 1 全部模型或替换 ID 算法。
- GroupNode 的选择集合、折叠状态、标签颜色等编辑器状态持久化。

## 7. Agent 并行与文件所有权

本 Task 由单一主 Agent 负责领域 API 与最终集成。可并行的只读子任务限于：审阅现有 Task 1 API/约束，或审阅真实 3.2.0.1 样本中与节点/图集相关的字段；不得让并行 Agent 同时改 `domain/`、`tests/` 或同一文档。主 Agent 冻结接口和 fixture 后方可拆分独立测试/文档工作，并由主 Agent 运行全部验收。

## 8. 交付清单

1. Qt 无关的 Scene/Root/Node/Group/Sprite 核心模型及公共 API 文档。
2. 约束、操作原子性和稳定诊断码说明。
3. 自动化测试与可复现验证记录。
4. 兼容 Task 1 API/ID 的说明；未实现的磁盘格式迁移明确标注为后续任务。
5. 更新后的 `docs/scene-system/SCENE_SYSTEM_SPEC.md`、`STRUCTURE.md`、`DECISIONS.md` 与本 Task 状态报告。
6. 未决问题、风险和 Task 3 建议输入。

完成后停止并提交验收，不自行开始 JSON codec、Spine 导入或 GUI 工作。
