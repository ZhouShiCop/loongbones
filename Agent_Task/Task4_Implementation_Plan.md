# Task 4 阶段 B：具体实现方案（提案）

- 状态：**B0 已复核通过；B1 实现与验证完成；B2/B3 实现完成；B4 Reader 与 27 项测试完成，全语料 Full 1113/1113、Primary 963/963 通过（交付边界＝结构可读）；B5 Atlas Reader + Mapper 与 32 项测试完成，全语料 region 未命中 0；B6 实现与验证完成，待负责人复核**。
- 前置：Task 4 阶段 A 已复核通过；本文件把批准的 RFC 拆成可执行的依赖顺序，不改变其语义。
- B0 已完成并经负责人复核通过。B1、B2、B3 已完成实现。B4 已完成：附件类型 0..5 全部可遍历，manifest 16 个锁定样本 16/16 一致。B5 已完成：图集文本解析提出为公开 API、Source→canonical 映射落地，语料 1113 个单位 region 未命中 0。B6 已完成：静态插件契约 + 注册表 + 事务提交 + 端到端闭合，25 项测试通过，7 个 CTest 目标绿，报告见 `docs/skeleton/B6_REPORT.md`。

## 1. 交付目标

按数据类型逐层完成无 GUI、可自动验证的导入闭环。第一条闭环只处理图集与图片；骨架及其附件随后接入：

```text
.skel.bytes + .atlas.txt + 授权读取的 PNG
    → Spine 插件解析（首片仅 Atlas 文本 + PNG；后续才读取 skeleton）
    → Loongbones canonical 候选（首片 Atlas + TextureAsset；后续 Skeleton）
    → 展示/检查 ImportReport
    → 宿主验证并原子提交候选
    → 当前可表达 Atlas/Texture 的原生工程 + 项目内纹理（B1 使用现有 v1 Codec；v2 Skeleton Codec 属 B3）
    → 重新打开并语义比对
```

Spine Reader 和 Spine 源 IR 只存在于 `importers/spine32/`；Domain、Format、宿主公共 API
不得包含 Spine 字段名、二进制类型或源格式对象。插件不生成第二种项目文件，不写 Spine/DragonBones
源格式作为中间数据。Task 4 不实现 GUI、场景实例、渲染或动画求值。

**数据研究方法与禁止项**：顺序固定为「观察原始数据 → 提出结构/语义假设 → 用合成样本和真实样本交叉验证 → 记录证据与反例 → 编写独立解析器 → 映射到 canonical 模型」。Spine Runtime、DragonBones Runtime、运行时库及其序列化器不得成为产品依赖、解析器、映射器或兜底路径；不得复制或翻译其实现。已有探针、旧 Reader 和运行时文档最多作为待核对的旁证，不能代替原始数据分析、独立解析与 golden 测试。

## 2. 依赖关系与工作包

按顺序完成下列工作包。除 B0 的文档冻结外，后续可由本地 Agent 分开执行；同一工作包不可并行改同一公共头文件。

### B0 — Canonical 数据类型、v2 Schema 与导入资源策略冻结

**先做文档，不写代码。**

1. 先复核并冻结 `docs/skeleton/canonical-visual-data-types.md` 的类型边界：图像文件资源 `TextureAsset`、页坐标平面 `AtlasPage`、打包切片 `AtlasRegion`、场景实例 `SpriteNode`、骨架绘制附件 `RegionAttachment` / `MeshAttachment`。不得把“图片文件、图集区域、场景图片节点、骨架附件”合并成一个类型。
2. 冻结 2D 模型的几何表达：Mesh 顶点、三角索引、regionUV 的归属；非加权与加权顶点语义分开。当前 Task 4 不实现 Mesh/WeightedMesh，只固定扩展边界，具体数值映射须有 Spine 3.2.01 golden 证据后另行纳入任务。
3. 对每个后续功能按 `canonical-visual-data-types.md` §7 建“功能目标 → Spine/DragonBones 源数据证据 → 运行语义 → Loongbones 契约 → 类型分层 → 转换/降级 → 演进与测试”分析表。类型可以多层、可以由 Loongbones 自行设计；不得因为源格式使用单一对象就强行一对一映射。绑定/蒙皮作为后续案例，先判断 BindingBase、BindingValue/Influence 与 AutoBind 分别应是数据、值还是操作，结论须经样本和原型验证后再冻结。
4. 遵循“最小基础、稳健扩展”：首期只实现已验证的必要类型；但稳定 ID、引用/所有权、unknown-field 锚点、能力声明与版本演进规则必须预先闭合。扩展不得改变既有字段含义；禁止为未验证能力预造空泛通用层，也禁止把源格式专属字段直接塞进 canonical 类型。
5. 新增 `docs/format/native-project-v2-skeleton.md`（或并入一个正式 v2 RFC）：列出 v2 完整 JSON 示例、字段必需性/默认值、枚举字符串、ID 类型、未知字段锚点、required capability、迁移和错误码；只序列化已冻结的首批类型，后续 Mesh 等通过显式 capability/版本演进。
6. 明确 Skin、SkinEntry、Attachment 的 ID/未知字段宿主规则。RFC 当前的 Skin entry 形如 `(slotId, attachmentName, attachment)`，但若该数组项可重排且无稳定宿主，Format 未知字段可能随索引错绑。建议给 entry/attachment 稳定 ULID，并为它们新增 `OwnerKind` 锚点；若选择无 ID 方案，必须证明编辑/重排后未知字段仍能正确归属。
7. 明确资源落地策略。**建议**把每个图集页 PNG 导入工程内，存为项目相对路径（例如 `assets/textures/<TextureAssetId>.png`），避免工程保存后依赖原始游戏目录。经宿主 `IResourceResolver` 只读取源 PNG，由宿主资源暂存服务写入；插件自身不获得任意路径写权限。
8. 固定 source hash 的语义：`sourceHeaderHash` 是可选源字段；`contentHash` 必须由宿主对 `.skel.bytes` 全文计算 SHA-256；纹理摘要不进入工程。
9. 更新 Task 4 Stage B 验收范围：未进入该切片的数据只产生统计与诊断，不创建半成品 Domain 对象；unsupported 为 warning，结构损坏/非法引用/缺纹理/非有限浮点为 error；warning 候选仅在用户明确确认后可提交。

**B0 验收**：Schema 示例可由读者独立实现；所有数组对象的 ID 与未知字段宿主确定；资源 URI/复制事务、诊断等级及格式迁移的保存时机没有待定语义。负责人复核 B0 后才进入代码工作包。

### B1 — 首个垂直切片：Atlas + PNG（不读取骨架）

先验证已有类型是否足够承载真实数据，不新增 Skeleton Domain。

1. 解析 `.atlas.txt` 的 page/region 与页头引用的 PNG；PNG 读取仅提取并校验格式、尺寸，不要求首片完成像素解码或渲染。
2. 创建 `TextureAsset`、`AtlasPage`、`AtlasRegion`，保存为 Loongbones 原生工程并重新打开做语义比对；页声明坐标尺寸与 PNG 像素尺寸分别保留，缩放只在页层表达。
3. 覆盖多页、rotation、trim、PNG 缺失/尺寸异常、路径授权边界、失败原子性；首片不读取 `.skel.bytes`，不创建 Skeleton/Bone/Slot/Skin/Attachment。
4. 将资源复制到项目内相对路径；源目录只读。B1 提供最小 staging/publish/rollback 闭环，B6 再验收完整插件宿主事务。

**B1 验收**：合成 Atlas+PNG fixture 导入、保存、重开后字段一致；非法输入不改变工程快照、不留下悬挂引用；现有 core/format 测试无回归。此切片先验证资源与 Atlas 数据链，之后才进入骨架模型。

**B1 实际实现说明（2026-10-20）**：位于 `importers/spine32/`，只读 Atlas 文本与引用 PNG；支持原始 Atlas 文本及本地语料出现的 Unity `TextAsset` 包装、页属性后无空行的页/首 Region 边界、多页、`rotate`、`xy/size/orig/offset/index`。不持久化的 `format/filter/repeat/pma` 等属性产生 warning，warning 未明确接受时不写盘。PNG 校验 signature/IHDR/CRC/尺寸，不解码像素。B1 输出使用现有 v1 Codec，因为本切片不含 Skeleton，且当前 v2 Codec 属 B3；没有修改 v1 Schema。资源先暂存并发布，project.json 保存失败时回滚新资源。详见 `docs/skeleton/B1_REPORT.md`。

### B2 — Domain Skeleton 模型与全量校验

建议新增：

```text
domain/include/loongbones/domain/skeleton.hpp
domain/src/skeleton.cpp
tests/domain/test_skeleton_*.cpp
```

1. 在 `IdKind` 增加 Skeleton/Bone/Slot/Skin/SkinEntry/Attachment 等必要种类；继续使用宿主随机 ULID，不按名称、索引或文件 hash 派生。
2. 新增 RFC §5 范围内的 canonical 类型：`Skeleton`、`Bone`、`Slot`、`Skin`、Skin entry、`RegionAttachment` 与 `SourceRef`。Mesh/IK/动画不进入 Domain。
3. 在 `ProjectData` 增加 `skeletons`；`Skeleton` 只由 Project 拥有，`Skin` 只嵌套于 Skeleton。
4. 扩展 `Project::validate()` / `adoptData()`，按候选副本先校验再提交。覆盖 ID 唯一、ULID 合法、根骨数量 ≥ 1 且允许多个、父引用/成环、slot→bone、skin→slot、entry/attachment 唯一、drawOrder 排列、Region→AtlasRegion、有限浮点与跨集合引用闭合。
5. 领域 API 继续 Qt-free；不提供可绕开完整校验的公开可变集合。

**B2 验收**：新增 Domain 单测覆盖每条拒绝规则、合法最小骨架、随机 ID 和失败后接收对象快照不变；Task 1/2 既有 API/语义不变。

### B3 — Native Format v2 双版本 Codec 与迁移

**状态（2026-10-21）：已实现。** 产物见 `docs/skeleton/B3_REPORT.md`。

改动限制在 `format/`、Domain 的 v2 必需字段和格式测试；不动已审阅的 v1 RFC，新增 v2 RFC 与迁移测试。

1. `ProjectDocument` 增加文档版本状态；增加 Skeleton/Bone/Slot/Skin/Entry/Attachment 对应的未知字段 `OwnerKind`。
2. Reader 同时接受 v1 与 v2：先判 `formatVersion`，再判 `minimumReaderVersion`，再查 required capabilities。v1 Reader 行为与 D24 完全一致；不支持的 required capability 返回 `CAPABILITY_UNSUPPORTED`。
3. v1 文档先完整解码/校验，再通过纯函数迁移为内存中的 v2 文档：只补 `skeletons: []`、设置 v2 版本/能力字段；不生成 ID、不改旧字段、不碰磁盘。
4. v2 Reader 严格解码 Skeleton 并走 Domain `adoptData()`；未知字段锚点以对象 kind + 稳定 ID 为主，不使用数组位置作为锚点。
5. 新写入统一输出 v2，带 `minimumReaderVersion: 2` 与 required `loongbones.skeleton.v1`；打开 v1 后只有用户显式保存才写 v2。保持现有同目录临时文件、回读校验、原子替换流程。
6. 若有 Skeleton/Entry/Attachment 嵌套的新未知字段，测试重排/重命名/删除宿主时未知字段跟随或丢弃语义符合 RFC。

**B3 验收**：原有全部 v1 fixtures 仍可读取；v1→内存 v2→未保存退出时原文件逐字节不变；v1→v2 编码→重开语义一致；unsupported version/capability 各报规定错误码；未知字段跨重排往返正确；core_tests 与 format_tests 无回归。

### B4 — Spine 二进制 Reader（仅解析，不映射）

**状态（2026-10-21 完成）：Reader 代码与 27 项自动化测试完成；全语料 Full 1113/1113、Primary 963/963 精确到达 EOF，0 失败。** 附件类型 0..5 全部可遍历（3.2 的类型顺序与官方 3.5+ 不同，加权顶点骨数为 float），6 及以上仍保持明确拒绝、不得猜测跳过。

**交付边界＝结构可读，不等于可转换**：只有 region 附件与头/骨/槽/皮肤骨架进入 IR；mesh 族、boundingbox、全部动画时间线、IK/transform 参数只遍历并计数。B5 按 `docs/skeleton/task4_B4.md` §9 的交接清单裁定导入范围，核心约束是**不得静默丢失**。详见 `docs/skeleton/task4_B4.md`。

建议新增：

```text
importers/spine32/include/loongbones/importers/spine32/binary_reader.hpp
importers/spine32/src/binary_reader.cpp
importers/spine32/src/source_model.hpp
tests/importers/spine32/test_binary_reader.cpp
```

1. 建立与 Domain 无关的私有 `SourceSkeleton` IR；实现固定宽度整数、变长整数、长度前缀字符串、颜色、float32 等有限小 Reader 原语，每次读取都携带 offset 和字段段名。
2. 所有读取前检查剩余长度、长度上限和整数溢出；所有 float32 立即 `isfinite`；文件必须 strict EOF；版本只接受头版本 `3.2.01`。
3. 每个源索引在 Reader/结构验证层检查范围与引用闭合；诊断带稳定 code、段名、字节 offset。未知附件类型明确拒绝，不猜测。
4. 实作前为每个字段留分析记录：样本/fixture、字节范围、解码假设、语义解释、反例检查、映射目标和未决点。未经证据验证的字段不得直接写入 canonical 模型。
5. 旧项目 Reader、Spine Runtime 文档或其它格式工具只可作旁证；不得复制代码、链接其模块、调用运行时 API 或用其输出代替本解析器的 golden 断言。Spine Runtime 不加入构建依赖。

**B4 验收**：5 个合成 fixture 预期均成立；测试截断、超长字符串、变长整数边界、负/越界索引、前向引用、自环、NaN、±Inf、尾随字节、unsupported version；对 manifest **16 个**锁定样本只读全量解析并分 Full/Primary 报告通过数/失败分类。旧 Reader 的“能读完”不是验收证据。

**实测（2026-10-21）**：27 项自动化测试通过；Full **1113/1113**、Primary **963/963**；manifest **16/16** 一致；扫描工具退出码 0。验收口径与退出码定义见 `docs/skeleton/task4_B4.md` §5.1 —— 其中「锁定样本 0 匹配必须返回非 0」是本轮补的守门规则，用于防止语料根给错时把无效扫描当成通过。

### B5 — Spine Atlas Reader 与 Source→Canonical Mapper

建议新增 `atlas_text_reader.cpp`、`mapper.cpp` 和对应测试，保持它们只依赖插件私有 IR、Atlas Domain API 与宿主服务。

1. 解析页名、页声明 size/format/filter/repeat/pma、region 名/xy/size/orig/offset/rotate；支持一份 atlas 的多页结构，虽语料没有真实多页样本，也用合成文本覆盖。
2. 页头 `file` 通过 `IResourceResolver` 解析为实际 PNG；读取 PNG IHDR 得到像素尺寸；页坐标尺寸仍使用 atlas 声明值，缩放仅由 `pixel/page` 记在页层约定中，不缩放 Region 坐标。
3. `rotate:true` 映射为 90°；trim/original 方向保持 atlas RFC 契约；对照现有 `AtlasRegion` 校验，不另造第二套区域矩形规则。
4. Mapper 用宿主 `IIdFactory` 分配所有持久化 ID；在单次导入局部映射表中把源索引/名称解析成 ID。`path ?? name` 精确匹配优先；尾随空格降级策略按已批准 RFC 处理并给 warning。
5. 对 unsupported 的每类能力给计数 + 首个源位置，不生成伪空 Domain 数据；分清 warning（用户确认后可导入）与 error（整个候选拒绝）。

**B5 验收**：所有 RegionAttachment 能解析到已存在 AtlasRegion；rotation/trim/非方形区域、图集多页、缺纹理、path 命中/未命中、尾随空格均有合成断言；页尺寸与 PNG IHDR 分开存储。

**实测（2026-10-21）**：32 项测试通过（atlas Reader 17 + Mapper 15），含「映射结果过 `Project::adoptData()`」的闭环断言。语料扫描：1113 个单位，进入映射 1111（2 个缺 `.atlas.txt`），**region 附件 27,457 全部命中、未命中 0**；27,457 + 20,252（非 region 跳过）= 47,709，与 `field-mapping.md` §10.1 的查找总数精确吻合。详见 `docs/skeleton/B5_REPORT.md`。

**边界**：B5 不改变 B4 的「结构可读」结论。20,252 个非 region 附件与全部动画 / IK / transform 只计数 + 诊断，不进 canonical；「能映射 region」不等于「能渲染网格」。

### B6 — 静态插件宿主、事务提交与端到端闭环

建议目录：

```text
importers/api/       # ImportPlugin、ImportRequest/Result、报告/诊断、取消/进度抽象
importers/host/      # 静态注册表、候选合并校验、确认后提交
importers/spine32/   # Spine 3.2.01 实现
tools/skeleton-import-smoke/  # 无 GUI 的演示宿主/端到端验证入口
tests/importers/     # 宿主事务与插件集成测试
```

1. `ImportPlugin` 接口只使用值语义 canonical 类型和宿主抽象；静态注册 `spine32`，descriptor 精确匹配 `spine.binary@3.2.01`。不创建运行时动态库/ABI。
2. 导入流程先生成 `ImportDraft` 和报告，完全不改目标 Project；错误诊断直接拒绝，warning 需调用方显式 `acceptWarnings` 后才可继续。
3. 提交采用完整候选副本：复制现有 `ProjectDocument`，追加 Skeleton/Atlas/Texture/资源引用，用临时 `Project::adoptData()` 校验全工程；成功后再生成待保存文档，不在插件解析过程中增量调用 Project 写 API。
4. PNG 由宿主资源暂存服务写到工程内的随机 ID 相对路径。先写临时资源并验证 hash/尺寸，候选 Domain 校验通过后再落资源文件，最后使用现有原子 `saveProject()` 替换 project.json；失败回滚新增文件且旧工程保持有效。进程崩溃最多遗留未引用文件，不得留下 project.json 指向缺失文件的窗口。
5. 无 GUI 期间使用 `skeleton-import-smoke` 作为最小宿主：传入 fixture/用户显式语料路径，打印报告，提供“确认 warning/取消”选项，保存后 reopen 并输出语义摘要。该工具不复制或提交真实游戏输入。

**B6 验收**：导入成功→保存→关闭/重开→对象计数与关键字段一致；取消/错误/warning 未确认时工程数据快照与磁盘文件保持不变；写资源或 JSON 失败时不产生悬挂引用；Resolver 拒绝越出授权根目录/符号链接逃逸；重复导入创建全新 ID 的副本。

## 3. 测试矩阵与阶段 B 总验收

| 层 | 主要证据 |
|---|---|
| Domain | 骨架不变量、ID、全工程 adopt 原子性、现有 Scene/Atlas 回归 |
| Format | v1 兼容、v1→v2、v2 严格读写、能力/版本拒绝、unknown anchors、保存原子性 |
| Binary Reader | 5 合成 fixtures + 16 个锁定语料；strict EOF、边界、索引、非有限数值 |
| Atlas/Mapper | region/rotation/trim/page scaling/资源缺失/path 映射/unsupported 报告 |
| Host/IO | warning 确认、取消、失败快照、资源 staging rollback、重复导入副本 |
| End-to-end | manifest 样本 #02（最小）、#03（深层级）、#11（压力）完成导入→保存→重开→语义比较 |

全程使用干净构建目录运行 core_tests、format_tests、全部 importer tests 与 smoke tests；报告命令、构建器/编译器、各层总数/通过/失败/跳过、退出码和警告。视觉正确性不在本任务验收内。

## 4. Agent 并行边界

- **串行先行**：负责人确认 B0；先完成 B1 图片/图集切片，再稳定 B2 Skeleton Domain 公共类型与 ID。
- **可并行**：B3 Format codec、B4 Binary Reader、B5 Atlas Reader/Mapper 可在对应接口文件冻结后分支工作；每块使用不同目录/文件，不并行改 `ProjectData`、`ProjectDocument`、CMake 根入口。
- **集成负责人**：唯一负责人集成 B3/B4/B5，处理 CMake/公共头冲突，执行 B6 端到端与干净构建。
- **不可并行的风险点**：v2 schema、未知字段 OwnerKind 锚点、导入资产事务与错误码必须先冻结，禁止 Agent 各自决定。

## 5. 后续可交付垂直切片

Atlas+PNG 首片通过后，再以 01-minimal + 02-nested 合成骨架 fixture 打通 B2→B6 的静态 RegionAttachment 闭环；之后加入畸形 fixtures 和 3 个真实 golden 样本。Mesh/WeightedMesh、约束、动画另立任务与验收，不与 RegionAttachment 首批混做。禁止一开始将 1113 个全量语料问题混入结构实现；全量语料扫描用于覆盖率报告，不代表所有样本必须零 warning（unsupported 需按支持矩阵统计）。

## 6. 源格式分析与安全边界

- Spine 与 DragonBones 都是外部输入格式，和 Loongbones 原生格式严格分离。产品只保存 canonical 数据，不保留源运行时对象或源序列化数据作为隐藏主数据。
- Task 4 只覆盖已批准的 Spine 3.2.01；DragonBones 的格式分析与导入器必须独立立项。可复用“先观察、再验证语义、后映射”的工程方法，不复用 Spine 假设或实现。
- 原始语料只读；仓库仅放可再分发的合成 fixture 和不含资产的样本元数据/hash。分析记录须区分事实、推断和未验证假设，并为进入首批模型的字段提供自动化边界/反例测试。
- 解析器只实现已观察且已验证的字段与版本。未知/不支持字段按获批支持矩阵显式诊断；不得猜测、静默丢弃、伪造默认对象或调用 Runtime 补全。

## 7. 阶段 B 授权与状态

| 工作包 | 授权/状态 |
|---|---|
| B0：类型、v2 Schema、资源策略冻结 | 已完成并复核通过 |
| B1：Atlas + PNG | 已获准；实现与验证完成，**负责人已核验通过（2026-10-21）** |
| B2：Skeleton Domain 与全量校验 | 已实现并核验通过；本轮确认根骨至少一个且允许多个 |
| B3：v2 Codec 与迁移 | 由其他 Agent 实现完成（2026-10-21）；三套测试通过并核验通过，报告见 `docs/skeleton/B3_REPORT.md` |
| B4：Spine Binary Reader | 已实现并核验通过：Reader/27 项测试；Full 1113/1113、Primary 963/963、manifest 16/16；交付边界＝结构可读（见 `task4_B4.md` §9） |
| B5：Atlas Reader + Mapper | 已实现并核验通过：Atlas 文本 Reader + Mapper 测试，全语料 region 未命中 0，报告见 `docs/skeleton/B5_REPORT.md` |
| B6：静态插件宿主与端到端闭环 | **已实现，待复核**：宿主事务 10 项 + 插件集成 15 项测试；语料 1113 单位跑过 prepare，**成功 1111 / 失败 2**（共享图集布局），region 未命中 0，8 个单位提交+重开一致，报告见 `docs/skeleton/B6_REPORT.md` |

阶段 B 的执行继续遵循本方案的工作包边界。一个工作包的授权不自动授权后续工作包 —— 上表中 B5 / B6 的「已完成」是**它们各自单独获批后**完成的结果，不是 B4 走查的连带授权。

当前（2026-10-21）：**B0–B5 已由负责人核验通过；B6 是唯一待复核的工作包**。复核通过后阶段 B 即全部收尾，
按 `Task4.md` 的阶段定义「完成后停止，等待复核」，**不自行启动下一阶段**。
