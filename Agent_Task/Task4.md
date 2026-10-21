# TASK 4：Spine 3.2.0.1 导入插件与骨架数据契约

- 状态：`阶段 A 与 B0 复核通过`；**B1–B5 已由负责人核验通过（2026-10-21）**；**B6 实现与验证完成、待复核**（阶段 B 唯一未复核的工作包）
- 阶段 A 产物：`docs/skeleton/`（Domain 与兼容提案、插件宿主契约、字段映射表、样本 manifest、合成 fixture、验收报告）+ `tools/skeleton-probe/`（只读探针，4 个 `.mjs`）
- 返修记录：第一轮见 `docs/skeleton/ACCEPTANCE.md` §7；第二轮见 §8；第三轮见 §9
- **B2（2026-10-20 实现完成）**：新增 `domain/include/loongbones/domain/skeleton.hpp`（Skeleton/Bone/Slot/Skin/SkinEntry/RegionAttachment/SourceRef/BlendMode/ColorF + `IdKind` 五种新种类）与 `domain/src/skeleton.cpp`（校验判据集中在 `validateSkeleton()`，与将来的骨架写 API 共用）；`ProjectData` 增加 `skeletons`，`validate()` / `adoptData()` 覆盖全量规则。根骨契约为至少一个且允许多个，已在 v2 Schema 明确。新增 13 个诊断码（`SKELETON_*`）。测试 `tests/domain/test_skeleton.cpp` 覆盖每条拒绝规则、合法最小骨架、随机 ID、诊断定位与快照不变。**未实现**：Spine 二进制解析（B4）、骨架写 API（写 API 属后续包）
- **B3（2026-10-21 实现完成）**：`format/` 支持 v1/v2 双版本读取（先 `formatVersion` → `minimumReaderVersion` → required capabilities），v1 完整解码并经 `migrateV1ToV2()` 纯函数迁移为内存 v2（不生成 ID、不改旧字段、不碰磁盘），写出**统一**为 v2（`minimumReaderVersion: 2` + required `loongbones.skeleton.v1`，`project.skeletons` 键恒存在）。`ProjectDocument` 增加 `sourceVersion` 与 5 个骨架 `OwnerKind`（Attachment 不设独立 kind，锚点 `(kSkinEntry, entryId)+/attachment`）。新增 23 个测试 `tests/format/test_v2_skeleton.cpp`（构建入口仍在 `tools/format-tests/`）。既有 `format_version_above_reader_rejected` 的过高值 2 → 3（v2 已被接受，属预期变更）。产物见 `docs/skeleton/B3_REPORT.md`。测试：core 116 / format 70 / atlas-import 8 全绿。**注意**：`ProjectData` / `ProjectDocument` 布局变更后必须全量重编，增量链接会造成 ODR 违例（表现为堆损坏）
- **B4 状态（2026-10-21 完成）**：Reader 与 27 项测试完成。全语料 **Full 1113/1113、Primary 963/963** 精确到达 EOF，0 失败；manifest **16 个**锁定样本骨/槽/皮肤/region **16/16 一致**；扫描工具退出码 0。附件类型 0..5 全部可遍历，6 及以上仍显式拒绝；加权顶点 `boneCount`/`boneIndex` 为 float（非官方 varint 写法）已由语料证伪并修正。
  **交付边界为「结构可读」而非「可转换」**：只有 region 附件与头/骨/槽/皮肤骨架进入 IR；mesh 族、boundingbox、全部动画时间线、IK/transform 参数**只遍历并计数，数据读完即弃**。不得据此宣称可渲染网格 / 可蒙皮 / 可播放动画。导入哪些、丢弃哪些及如何诊断，按 `docs/skeleton/task4_B4.md` §9 的交接清单由 B5 裁定（§9.3 三条硬约束，核心是**不得静默丢失**）。
  扫描工具的验收口径与退出码见 `task4_B4.md` §5.1：锁定样本 0 匹配时**必须**返回非 0（实测给错语料根 → 扫到 1131 个文件、0/0 匹配、退出码 4）。
  **B5/B6 此后已分别获准并完成**：B5 已核验通过（见 `docs/skeleton/B5_REPORT.md`）；
  B6 实现与验证完成、**待负责人复核**（见 `docs/skeleton/B6_REPORT.md`）。
- **B5（2026-10-21 实现完成）**：新增 `atlas_text_reader.{hpp,cpp}`（`.atlas` 文本 → 插件私有 IR）与 `mapper.{hpp,cpp}`（Source → canonical `Skeleton`）。B1 里文件内私有的图集解析器已提出为公开 API 供 B5 复用，B1 行为不变。
  ID **全部**由宿主 `IIdFactory` 发放，源下标只在本次导入的局部映射表里解析。附件 region 查找键 **`path ?? name`**：精确命中正常建条目；未命中可按 `trim()` 重试并记 `ATTACHMENT_REGION_DEGRADED`（warning，T-B3）；仍未命中是 `ATTACHMENT_REGION_UNRESOLVED`（**error**，拒绝候选且不留半成品）。
  **不得静默丢失**：B4 只计数的能力（mesh 族 / boundingbox / 动画时间线 / IK / transform / 事件）**只产出计数与 warning，不建任何 Domain 对象**。
  测试 32 项（atlas Reader 17 + Mapper 15），含「映射结果过 `Project::adoptData()`」的闭环断言。语料扫描（`tools/spine32-mapper-smoke/`）：1113 个单位进入映射 1111（2 个缺图集）、**region 附件 27,457 全部命中、未命中 0**；27,457 + 20,252（非 region 跳过）= 47,709，与 `field-mapping.md` §10.1 的查找总数精确吻合 —— 即该文档记录的 4 例未命中落在**非 region** 附件上，本阶段不映射。产物见 `docs/skeleton/B5_REPORT.md`。
  **边界**：B5 不改变 B4 的「结构可读」结论 —— 无任何动画 / mesh / 蒙皮 / IK 进入 canonical。
- **B6（2026-10-21 实现完成）**：新增 `importers/api/`（导入契约：插件接口 / 请求-草案-报告 / 诊断码 / 5 个宿主服务接口 / 路径与 SHA-256 工具）与
  `importers/host/`（`ImportRegistry` 静态注册表 + `commitImport()` 事务提交）。`importers/spine32/` 新增
  `spine32_import_plugin.{hpp,cpp}`（`prepare()`：二进制 → 图集 → 映射 → 域校验）与 `builtin_registration.cpp`
  （`registerBuiltinPlugins()` 的**插件侧**定义 —— 不用静态初始化器，因静态库里只有初始化器的目标文件
  不会被链接器拉入，注册会**静默丢失**）。
  依赖方向恒为「插件 → 宿主」：宿主库不 `#include` 任何 `spine32` 头。文件系统的 `IResourceResolver` /
  `IResourceStaging` 实现仍留在插件侧，宿主只依赖接口。
  事务顺序 = stage（写临时文件并回读比对）→ publish（原子安装、拒绝覆盖）→ 原子 `saveProject()` → `complete()`；
  任一步失败只回滚本事务创建的文件，旧工程保持有效；析构函数兜底回滚未 complete 的事务。
  测试 25 项（宿主事务 10 + 插件集成 15），含「提交后**重开**工程对象计数一致」的闭环断言。
  语料端到端（`tools/skeleton-import-smoke/`，默认 OFF）：1113 个单位 prepare 成功 1111（2 个共享图集布局），
  **region 未命中 0**，完整提交 + 重开 8/8 一致。产物见 `docs/skeleton/B6_REPORT.md`。
  **已知缺口**：共享图集布局（多个骨架共用一个与目录同名的 `.atlas.txt`）不支持 —— 需要给 `IResourceResolver`
  增加目录列举能力，超出 B6 范围。
- **数据类型报告（2026-10-21）**：已实现 canonical 类型总表、被误删重建文件的符号/字段逐一核对结果
  （4 个自由函数 + 2 个类 10 个成员函数**零缺失**）、后续待实现类型（mesh 族 / 动画 / 约束 / 事件，
  语料实测 `timelines` **4,988,424**）与残余风险，见 [`DataType_Report.md`](DataType_Report.md)。
- **构建产物位置变更（2026-10-21）**：历史 15 个 `build*/` 目录已整体移至仓库外
  `D:\Works\Spine\.builds\`；后续构建用 `-B ..\.builds\build`。
- **B0（2026-10-20 复核通过）**：canonical 类型边界与 2D 几何/UV 冻结、v2 schema 冻结、Skin/SkinEntry/Attachment 的 ID 与未知字段宿主规则、资源落地策略、source hash 语义、阶段 B 验收范围。复核确认 `sourceRef` 整个对象可选、每个 v2 文档要求 `loongbones.skeleton.v1`、保留 `SkinEntry.id`。四项功能分析证据只来自 Spine 3.2.01，尚未完成 DragonBones 对照；结论不得跨格式泛化，DragonBones 研究与导入独立立项。产物见 `docs/skeleton/B0_REPORT.md`、`docs/skeleton/canonical-visual-data-types.md`、`docs/format/native-project-v2-skeleton.md`
- 阶段 B 具体实现顺序（领域、v2 Codec/迁移、Spine Reader、Atlas Mapper、插件事务、测试与并行边界）见 [`Agent_Task/Task4_Implementation_Plan.md`](Task4_Implementation_Plan.md)。阶段 B 执行时以该方案为细化任务；本任务 §3–§4 为目标级边界。
- Canonical 视觉数据类型目录见 [`docs/skeleton/canonical-visual-data-types.md`](../docs/skeleton/canonical-visual-data-types.md)：首个实现切片先只导入 `.atlas.txt` + PNG，验证 TextureAsset/AtlasPage/AtlasRegion；不读取骨架。后续才接入 Skeleton/Bone/Slot/Skin/RegionAttachment；Mesh/UV 的扩展边界先记录，暂不实现。
- 追加裁定（2026-10-20）：含 `skeletons[]` 的 v2 文档必须声明 `loongbones.skeleton.v1` 为 **required**（不替代版本检查）；旧 v1 Reader 读 v2 固定返回 `FORMAT_VERSION_UNSUPPORTED`；源带头 `hash` 与文件 SHA-256 拆为 `sourceRef.sourceHeaderHash` 与 `sourceRef.contentHash` 两个字段
- 已裁定（D24–D28）：原生格式 v2 + 显式纯函数迁移、随机 ULID + 默认创建副本、编译期静态插件注册、Skeleton 作为 Project 资产且本任务不放置到 Scene、不提供插件版本迁移钩子
- 依赖：Task 1/2/3（含 Task 3 Back Task）；`项目规范.md`；`docs/mapping-spine32-to-native.md`；`docs/licenses-and-risks.md`；ADR-0002/0003/0004；`DECISIONS.md`
- 输入语料：`D:\Game\Other\sdorica\working\full_export\assets\game\character\spinedata`（只读）
- 目标项目：`D:\Works\Spine\Loongbones`
- 目标：以 Spine 3.2.0.1（Sdorica 私有分支）为首个**导入插件**目标，为骨架静态数据建立稳定、可演进的领域/原生格式契约，并完成一条可诊断的插件导入垂直切片。

## 1. 目标与边界

Task 3 已完成静态 Project/Scene/Node/Asset/Atlas 原生格式 v1 codec；该格式**不包含** Skeleton/Bone/Slot/Attachment/Animation。Spine/DragonBones 等外部格式与 Loongbones 原生格式不同：源格式只属于相应导入插件，Domain、项目保存和模块交换均使用 Loongbones 自己的 canonical model/native format。不得直接把这些字段塞入现有 Domain 或静默扩张 v1。Task 4 先形成可审阅的骨架数据契约与插件边界，之后才能实现 Domain 与导入插件。

本任务聚焦于：

- 识别并解析 `.skel.bytes`、`.atlas.txt` 与伴随图片的输入关系；目标版本严格限定 `3.2.0.1` / 实测头版本 `3.2.01`。
- 建立静态骨架可编辑模型及其与现有 Project/Asset/Atlas 模型的稳定映射。
- 完成 bones、slots、skins、region attachment 的最小静态导入闭环；对其他内容明确保留或报告，不得静默丢弃。
- 形成后续 mesh/weighted mesh、IK、animation/timeline、deform 与求值的兼容扩展路线，但不在本任务冒称已支持。

不在本任务：Qt GUI、渲染、动画求值、完整 Spine 编辑器、DragonBones、Spine Runtime 移植/翻译、修改原始 Sdorica 文件、将任何原始游戏资产提交到仓库。

**格式研究原则**：不以 Spine Runtime 或 DragonBones Runtime 作为解析实现、产品依赖或兜底路径。先观察只读源数据，分析字节结构和字段语义，以合成 fixture / golden 样本验证假设，再实现独立 Reader 并转换为 Loongbones canonical 数据。运行时文档、既有探针或旧 Reader 仅可作旁证，不能替代数据证据，也不得复制或翻译实现。DragonBones 遵循同一方法，但需独立立项、独立分析其格式；不得将 Spine 语义直接套用过去。

**插件定义**：本 Task 的“Spine 导入”必须作为可由宿主发现/调用的插件能力交付，不是把 Spine Reader 直接编进 Domain 或让应用核心依赖 Spine 字段。插件可解析 Spine 输入，但输出必须是 Loongbones canonical model，并由宿主的原生格式服务保存为 Loongbones 工程。Task 4 不实现 DragonBones 插件；未来是否支持其它源格式须各自立项。

## 2. 阶段 A：数据/契约冻结（本阶段完成后停下等待审阅）

阶段 A **不实现**骨架 Domain、不写产品导入插件、不修改 Task 3 已发布的原生格式契约。允许只读调查与独立的开发期探针；新增验证工具必须有同级 README/USAGE 并登记根 `STRUCTURE.md`。

### 2.1 源数据与样本基线

1. 阅读 corpus-scan README、manifest、样本选择说明、`docs/mapping-spine32-to-native.md`、旧项目自研二进制 Reader 和相关数据字典；旧项目代码只作为本团队格式读取器/证据参照，必须独立实现产品代码。
2. 对 Sdorica 输入保持只读。复用已建立的 Primary 963 / Full 1113 双口径，不混用分母；核实 Task 4 样本至少覆盖简单/复杂层级、多 skin、多 atlas 页、region rotation/trim、IK、mesh/weighted mesh、deform 与长动画。
3. 将样本 ID、SHA-256、相对路径（相对 corpus root）、版本、特性标签、文件伴随关系和解析结论写入可复现 manifest；不把机器绝对路径、用户名、图像、`.skel.bytes` 或 `.atlas.txt` 原始数据复制入仓库。
4. 核实二进制 reader 的 offset/length、严格 EOF、截断/非法索引/非法浮点诊断；核实 atlas 页与 region 名称、PNG 解析、同名/异名附件路径规则。未知或不确定项继续标为待验证，不猜测。

### 2.2 Skeleton Domain 与 Native Format RFC

新增 `docs/skeleton/` 或经负责人认可的文档位置，定义独立、Qt-free 的骨架模型 RFC。至少明确：

1. Skeleton 与现有 Project/Scene/Node/Asset/Atlas 的职责边界；不要把 Spine Bone 误当作 Scene Node，也不得改变 Task 1/2 已发布 ID 与语义。
2. Skeleton、Bone、Slot、Skin、Attachment、RegionAttachment 的字段、稳定 ID、命名/重复名策略、顺序、父级/索引引用校验、删除/重命名语义及跨对象引用。
3. Spine 的 setup pose、父骨索引、局部变换、Y-up、角度单位、颜色/混合模式、slot 绘制顺序、region trim/rotation、附件 path 与 atlas region 查找策略；每项注明证据来源、转换规则与 golden 值计划。
4. ID 策略须与 ADR-0004/已定案台账兼容：映射 ID 确定性、同一源重复导入行为、名称重名和原始索引变化的影响必须明确。不得用数组下标作为持久化对象 ID。
5. 与原生格式 v1 的兼容提案：比较可选扩展/能力声明、格式版本升级与迁移等方案；明确旧 Reader 打开、保留未知字段、保存、再次打开的行为。提交推荐项与迁移/降级风险，**不得由 Agent 自行修改已审阅 v1 RFC 或字段表**。
6. 未知/不支持 Spine 数据策略：区分“保留原始片段”“语义映射”“不支持且有诊断”；说明格式数据可安全保留的范围、诊断位置和可能的数据损失。不得把缺失能力伪装成空对象。
7. 产出支持矩阵：本任务首批可编辑能力、仅可解析/保留能力、明确拒绝能力、待验证能力；为后续 mesh/weighted mesh、约束、动画与 deform 列依赖及风险。

### 2.3 插件边界 RFC

阶段 A 必须提出最小导入插件宿主契约，至少覆盖：

1. 插件身份、版本、支持的源格式/版本、宿主 API 版本范围、能力声明与兼容拒绝行为。
2. 插件发现/启用/禁用、输入选择与调用、取消/进度、诊断回传、资源访问授权、重复导入策略；无论采用 Qt 插件、动态库还是其他方案，均先分析 ABI/工具链与版本升级风险并提交推荐，不得擅自冻结实现技术。
3. 插件输入为只读源文件/资源访问接口；输出为带导入报告的候选 canonical Domain 数据。插件不得创建自己的项目格式、绕过宿主校验/事务，或直接操作 Qt UI。
4. 插件故障不得破坏宿主工程；不支持的源版本/能力必须显式诊断。明确插件是否进程内运行及其信任边界，禁止宣称崩溃隔离而实际没有进程隔离。
5. 旧 Reader 与未来插件版本的行为：插件升级、宿主 API 不兼容、重复插件 ID、缺失依赖时如何拒绝/回退；插件元数据不与原生项目数据混为一体。
6. 源格式和 Loongbones 格式边界：导入后保存的文件应只依赖获批的 Loongbones 原生 schema；不得把 Spine/DragonBones 序列化作为隐藏主数据或内部中转格式。

### 2.4 阶段 A 验收与停点

- 交付 `docs/skeleton/` 下的 Domain/原生格式 RFC、插件宿主 RFC、字段/映射表、样本 manifest（只含元数据/hash）、风险与待决项；更新 `DECISIONS.md`，不得将未由负责人确认的建议写作已定案。
- 独立探针如存在，需可复现地对选定样本输出版本、对象计数、引用闭合、未知字段/诊断摘要；只读，不生成产品项目，不复制原始数据。
- 至少给出 3 个可再分发的合成 fixture：最小骨架、嵌套骨骼 + 多 Slot/Region、畸形引用/索引。fixture 不含游戏资产，覆盖规范与拒绝语义。
- 验收报告说明统计分母、样本选择、每项语义证据、失败分类、工具链、工具运行入口、未决决策与建议。
- 完成后停下等待负责人审阅。阶段 A 获批前，禁止新增 Skeleton Domain、修改 `format/` 持久化字段或开始产品导入实现。

## 3. 阶段 B：静态骨架导入垂直切片（阶段 A 获批后）

仅按获批 RFC 实施；若契约变更需另行批准，不得边写代码边固化格式。

### 3.1 模块与接口边界

- 新增 Qt-free `domain/` 骨架类型与验证 API；Domain 不依赖 RapidJSON、Spine Reader、Qt 或外部文件系统。
- 新增独立的 Spine 导入插件（建议 `plugins/importers/spine32/`，具体路径服从阶段 A 审批的插件契约），输入解码为独立源 IR，再经映射服务生成候选 Domain/Project 数据。
- 通过宿主插件 API 注册/发现与调用；Spine 二进制 Reader、atlas Reader、映射策略留在该插件模块内，Domain 和原生格式模块不依赖插件或源格式类型。
- 导入器只读源文件；解析、校验、映射均在候选对象上进行，失败不得污染现有工程或产生半导入结果。
- 项目保存经宿主的 Loongbones 原生格式公共 API；禁止导入插件私自写第二套 JSON、保存 Spine/DragonBones 原格式或制造互不兼容的中间工程格式。
- RapidJSON 仅服务原生 JSON；Spine 二进制与 atlas 文本解析必须由独立实现完成，不引入 Spine Runtime。
- Spine Reader 的字段实现必须有对应的数据观察记录、语义证据与可复现测试；证据不足的字段保持未决或显式诊断，不猜测、不调用 Runtime 补全。

### 3.2 首批实现范围

1. Spine 3.2.01 header/version/非 essential 标志和骨架基础元数据。
2. Bone 层级、setup transform、length、继承语义；Slot 的 bone 引用、setup attachment、颜色、blend mode 与顺序。
3. Skin 与 region attachment；附件 transform、尺寸、颜色、path；解析 atlas page/region 的尺寸、rotation、trim、页引用，并链接现有纹理/图集资源。
4. 对 IK、transform/path constraint、mesh/weighted mesh、动画、事件、deform 等未纳入本切片的数据，按阶段 A 决定的方式保留、显式诊断或拒绝；每个忽略/降级项带可定位来源，不得静默丢弃。
5. 产出导入报告：源文件 hash、解析器/映射版本、计数差异、诊断、未支持字段、缺失素材和映射警告。用户可明确选择“取消”或“按报告导入”；不得默认忽略数据损失。

## 4. 测试与验收

阶段 B 至少包括：

- 单元测试覆盖 header 版本/截断、字符串与变长整数边界、负/越界索引、非有限浮点、父骨循环/层级、重复名与 ID、Skin/Slot/Attachment 引用、Atlas/Region 匹配、rotation/trim、资源缺失与失败原子性。
- 对合成 fixture 做精确计数与字段断言；对选定真实样本只读运行并按 Primary/Full 口径分别报告覆盖率及失败分类，不把游戏原始数据写入仓库。
- 对至少 3 个特征不同的真实样本完成 `Spine source → Domain model → Native save → reopen → semantic comparison`；骨骼、Slot 顺序、皮肤、Region、尺寸与变换逐项比较，记录浮点容差和已知差异。
- 故意注入解析/映射失败，验证目标 Project 前后完整快照一致，无半成品实体或临时资源。
- 从干净构建目录运行新增测试及现有 `core_tests`、`format_tests`；报告精确命令、总数/通过/失败/跳过、退出码、警告和编译器。不得只运行旧二进制。
- 验证工具若新增/改动，提供就近 README 与 `STRUCTURE.md` 登记；更新能力矩阵、Task 报告和风险/决策状态。

## 5. 允许与禁止修改范围

阶段 A 允许：`docs/skeleton/`、`docs/mapping-spine32-to-native.md`（证据修订）、`DECISIONS.md`、`Agent_Task/Task4.md`、`项目规范.md`、`STRUCTURE.md`、新增只读探针及其 README/登记。

阶段 B 获批后允许：阶段 A 文档、`domain/` 骨架模型、获批路径下的 Spine 导入插件与宿主 API、经 RFC 批准的 `format/` 与迁移、合成 fixture、相关测试/CMake/工具 README/`STRUCTURE.md`。

始终禁止：修改/复制/提交 Sdorica 原始资源；把 Spine Runtime 源码移植或翻译进产品；未批准就扩张原生格式 v1；破坏 Task 1/2/3 公共契约；实现 DragonBones、Qt GUI、渲染或动画求值；顺带改写无关代码或全仓格式化。

## 6. 交付与状态

阶段 A：契约 RFC、映射矩阵、可复现样本元数据、合成 fixture、只读探针（如需）、决策/风险更新和审阅报告；完成后等待负责人批准阶段 B。

阶段 B：独立 Spine 3.2.01 导入插件、稳定宿主 API 与 Domain API、按批准契约将 canonical 数据保存为 Loongbones 原生工程、诊断/回归测试/README/`STRUCTURE.md`、完成报告与下一阶段建议。阶段 B 完成后停止，等待复核；不得自行宣称视觉效果通过。
