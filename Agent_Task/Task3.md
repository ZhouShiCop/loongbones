# TASK 3：原生项目格式 Schema v1 与 JSON Codec

- 状态：**阶段 A 复核通过；阶段 B 实现完成，待负责人复核**（Schema RFC v1.2 已获批；RapidJSON 已 vendor 并登记许可证；codec 与目录工程读写已落地）
- 依赖：Task 1、Task 2（含 Back Task）；`项目规范.md`；ADR-0002；ADR-0003；`DECISIONS.md`
- 目标项目：`D:\Works\Spine\Loongbones`
- 本 Task 交付：为当前静态 Project/Scene/Node/Asset/Atlas 模型冻结首版原生格式契约，并实现可保存、关闭、重开且保留未知数据的 JSON 目录工程垂直切片。

## 1. 目标与边界

原生格式是用户项目格式，也是应用内部交换格式。它必须忠实持久化 Task 1/2 当前模型，避免格式语义与领域模型分叉，并为后续只增不改扩展和迁移建立明确契约。

本 Task 采用两个阶段：

1. **阶段 A：Schema RFC。** 盘点当前字段、提交 schema/兼容 RFC 和可运行的格式探针；停止等待负责人审阅 RFC。不得先写入会形成持久化承诺的 codec 或修改领域模型。
2. **阶段 B：Codec 垂直切片。** RFC 获批后实现 v1 JSON codec、目录工程 `project.json` 读写、未知可选字段 round-trip、损坏数据诊断、原子保存及回归测试。

本 Task 的 v1 只包含当前已实现的静态模型：Project、Scene、Root/Group/Image(Sprite) Node、ImageAsset、TextureAsset、Atlas、AtlasPage、AtlasRegion、Transform、节点层级和默认 Scene。**不得伪造 Skeleton/Bone/Slot/Attachment/Animation/Timeline 空模型或临时字段来预占格式。** 这些对象、Spine 导入、求值与渲染另行立项；本 Task 的 RFC 必须明确它们不在 v1 的已支持能力中。

## 2. 必须遵循的已定契约

- 文件容器使用开发期目录工程，核心文档为 UTF-8 `project.json`；图片仍是独立资源。暂不压缩、打包或引入二进制块。
- 保持 Qt 无关；格式层置于 `format/`，依赖方向为 Format → Domain，禁止 Domain 依赖 JSON 库、Qt、文件系统或格式版本细节。
- 持久化 ID 使用字符串原值，不从名称、数组下标、路径派生；不因序列化、重排、改名重建 ID。
- 枚举用稳定字符串，不序列化 C++ 枚举整数；字段名统一小写 `camelCase`；数组顺序有语义的必须保序。
- Y-up、角度为度、矩阵为 2×3 仿射行主、`world = parent × local`；字段单位须明确。不要把 GUI 状态或运行时缓存写入 v1。
- `formatVersion` 与 `appVersion` 分离；核心字段类型、默认值和 required/optional 状态明确；未知可选字段须可读写往返保留。未知必需能力必须明确拒绝，不能打开后保存时静默丢失。
- 保存使用同目录临时文件、完整写入与校验、原子替换；解析/校验/迁移/保存失败不得破坏源文件或当前内存工程。
- 本格式只读写自有模型，不冒充 Spine、DragonBones 或其他格式。

## 3. 阶段 A：Schema RFC（先交审阅）

产出 `docs/format/native-project-v1-rfc.md`，至少定清：

1. 文档顶层结构和元数据：`formatId`、`formatVersion`、`minimumReaderVersion`、`appVersion`、`projectId`、能力/扩展声明及 `ProjectData` 内容的映射。
2. Task 1/2 每个持久化字段的稳定名称、类型、单位、默认值、可空性、必需性、枚举字符串、引用关系和顺序语义；覆盖 root 与单 Scene 的冗余兼容字段如何编码及如何一致性校验。
3. 资源位置字段的语义：保留外部引用与工程内资源两种存储模式；本 Task 不解码/复制图片，但不得使路径语义含混或把机器绝对路径默认为可移植路径。
4. JSON 数值规则（拒绝 NaN/Inf）、重复 key、非法 UTF-8、重复 ID、未知字段、未知能力、深度/大小上限和错误定位（JSON Pointer 或等价位置）。
5. 兼容规则：初始 v1 的版本表示、未来只增不改原则、旧 Reader 对新格式/必需能力的行为、迁移注册与执行顺序、失败时保留源文件、迁移的幂等性及回滚策略。不得虚构已有历史版本；如没有真实旧格式，写明当前无迁移输入，并给出新增版本时的迁移模板。
6. unknown-field 保留机制：对象级未知可选键如何随数据经过编辑后仍被原样语义保留；未知必需扩展如何标识并拒绝；定义“语义保留”而非要求空白/键顺序字节不变。对象可能位于可重排数组中，不能只用数组下标 JSON Pointer 作为持久所有者键；必须规定稳定归属与删除/重排行为。未知数值需保留精确原始字面量；不能由有限范围的 `double` 单独表示。
7. JSON 库候选及依赖评估：Qt-free、C++17、许可证、维护/版本固定方式、错误诊断能力、未知字段支持、构建与分发成本。提出推荐方案但不新增依赖；负责人审阅并批准 RFC 后才能进入阶段 B。
8. RFC 附 JSON Schema（或等价机器可读结构约束）、合法/非法小型 fixture，以及 `docs/adr/0002-native-format-container.md` 需补充或保持不变的决策说明。

阶段 A 验收：RFC 明确列出 v1 的完整边界和未决事项；可由他人仅凭 RFC 编写独立读写器；所有字段均能映射到现有 Domain API/数据；不存在未获批的破坏性领域模型改动。完成后等待负责人审阅，不自行进入阶段 B。

## 4. 阶段 B：实现与接口（RFC 获批后）

### 4.1 Codec 与目录工程

- 新增 Qt-free `format/` 模块及清晰公共 API：从字节/JSON 文档解码为候选 `ProjectData`，通过现有 `Project::adoptData()` 完整校验后交付；从有效 ProjectData 编码为确定性 JSON 文档。
- 提供目录工程最小服务：创建工程目录、打开其中 `project.json`、保存到原目录或另存到新目录。图片/资源复制、资源重定位、自动保存与单文件打包不在本 Task。
- 编解码必须覆盖第 2 节列出的所有当前持久化字段。字段值按 RFC 明确处理，不得丢节点、资产、Scene、页、Region、节点顺序或引用。
- 加载结果先构建候选对象并校验，全部成功后才替换接收对象；失败返回稳定诊断码和可定位字段路径，接收方完整快照不变。
- 编码输出确定性：对象键按固定 schema 顺序输出，向量按领域顺序输出，重复保存内容稳定（不要求随机临时文件名稳定）。未知字段按 RFC 语义保留。
- 原子保存需处理已有目标、写入失败、磁盘/权限错误、临时文件残留和 Windows 替换语义；源文件在失败路径保持不变。不得用“先删除旧文件再重命名”冒充原子替换。
- 路径内容只按 RFC 编码/保留；除明确获批范围外，不解析图片、不访问外部图片、不静默重写路径。

### 4.2 错误与兼容

- 新增诊断码只追加，不改变既有码含义；至少区分 JSON 语法/类型错误、必需字段缺失、版本不支持、必需能力不支持、Domain 数据校验失败、读写/替换失败。
- JSON 语法和类型错误要包含文件与字段位置；Domain 不变量失败复用现有诊断，并保留 location。
- 不支持的 `formatId`、高于当前 `minimumReaderVersion` 的文档、未知必需能力、无法迁移的版本必须拒绝，不能降级后丢字段。
- v1 没有真实历史迁移源时，不得发明无依据的数据转换；提交迁移接口/策略与 v1 fixture，并将第一条实际历史迁移留待真实版本演进。

## 5. 测试与验收

阶段 A 探针至少覆盖合法文档、缺字段、错误类型、未知字段、未知必需能力和格式版本边界，并可复现运行。

阶段 B 使用独立的 `tools/format-tests/README.md`（若新增验证入口，必须登记根 `STRUCTURE.md`），从干净目录构建。测试至少覆盖：

- Project/Scene/全部 Node 类型、父子顺序、Transform、ImageAsset、Texture/Atlas/Page/Region 的完整 encode→decode→encode 语义往返。
- 真实领域快照比较：ID、字段、数组顺序、引用、可选值、浮点值、资源位置全部不丢；未知可选字段在打开、修改已知字段、保存后仍存在。
- 对包含未知字段的节点/图集/区域执行重排或已知字段编辑后，未知字段仍归属原对象；删除宿主对象时只丢弃该对象的扩展字段。
- 未知数值覆盖超出 `uint64` 的整数、高精度小数及大指数合法 JSON 数（如 `1e400`），验证 raw token 精确保留；已知数值越界则明确拒绝。
- 错误 JSON、截断文件、重复 key、错误类型、缺必需字段、非法枚举、非法 ID、越界/不一致领域数据、未知必需能力、过新/错误格式版本拒绝且有可定位诊断。
- 加载失败不改变已有 Project 完整快照；保存失败不改变磁盘源文件；临时文件清理或可恢复；另存失败不破坏原工程。
- 创建→保存→关闭进程/重建对象→重开→改字段→再保存，内容一致；连续读写结果可重复。
- v1 合法 fixture、非法 fixture 与格式契约一致。合成 fixture 明确标注；不把 Sdorica 原始资产复制入仓库。
- 现有 Task 1/2 领域测试继续通过；格式模块不得向 Domain 引入 JSON/Qt 依赖。

报告：干净 configure/build、测试总数及通过/失败/跳过、CTest、退出码、编译警告、工具链和 JSON 依赖版本/许可证。不得只运行旧二进制。

## 6. 允许与禁止修改范围

允许：`docs/format/`、`docs/adr/0002-native-format-container.md`、`format/`、`tests/format/`、必要的根/子级 CMake 文件、`tools/format-probe/`、`tools/format-tests/`、新增诊断码、`Agent_Task/Task3*`、`STRUCTURE.md`、`DECISIONS.md`。每次新增工具同步更新 README 与 STRUCTURE 登记。

禁止：改写 Task 1/2 既有 ID 或公开语义；无 RFC 批准即修改 schema/domain；加入骨骼动画领域字段、Spine 解析器或 Spine Runtime；导出 DragonBones；Qt UI、渲染或图片解码；复制 Sdorica 原始数据；更改无关文件格式或批量重格式化。

## 7. 交付物与停点

阶段 A：RFC、机器可读 schema、fixture、探针和推荐决策，提交后等待负责人审阅。

阶段 B 获批后：Qt-free JSON codec 与目录工程读写、稳定诊断、原子保存、fixtures、自动化验证、README/STRUCTURE 登记、完成报告、未解决问题及后续 Task 建议。

Agent 不得因 RFC 文档已完成就自行宣布阶段 B 获批；也不得自行开始 Task 4 的 Spine 导入、完整骨骼/动画模型或 GUI。阶段 B 验收后停止并提交复核。
