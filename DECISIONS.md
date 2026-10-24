# 决策状态与待验证事项

本文件是**决策台账**：记录哪些已定案、哪些待验证、哪些属于后续阶段。
详细论证见 `docs/adr/`。**未在此标记为"已定案"的内容，不得当作既定事实实施。**

对应规范 `项目规范.md` §5 Phase 0 交付物 5、§12。

---

## 1. 已定案（可直接实施）

| # | 决策 | 载体 | 负责人决定日期 |
|---|---|---|---|
| D1 | 技术栈**原则**：C++17 + Qt Widgets + OpenGL + CMake；先做可撤销对比实验再固化；Domain 独立于 Qt；渲染后端接口隔离 | [ADR-0001](docs/adr/0001-tech-stack-qt-and-render-backend.md) | 2026-10-19 |
| D2 | 目录命名统一按**已更正的**目录名，**不保留两套大小写约定** | `STRUCTURE.md` | 2026-10-19 |
| D3 | **几何坐标系 = Y-up**，作为正式契约 | [ADR-0003](docs/adr/0003-units-and-coordinate-contract.md) C1 | 2026-10-19 |
| D4 | 角度单位 = **度**（字段名带单位），内部求值用弧度 | ADR-0003 C2 | 2026-10-19 |
| D5 | 矩阵布局 = **2×3 仿射、行主、`world = parent × local`** | ADR-0003 C9 | 2026-10-19 |
| D6 | 颜色/透明度 = **0..1 浮点** | ADR-0003 C8 | 2026-10-19 |
| D7 | 核心工程格式 = **JSON**，图片资源**独立存储** | [ADR-0002](docs/adr/0002-native-format-container.md) | 2026-10-19 |
| D8 | 开发期用**目录工程**，支持外部图片引用 + 工程内资源；未来可加单文件打包但**不得两套不兼容模型** | ADR-0002 | 2026-10-19 |
| D9 | **暂不压缩、不提前实现二进制容器**；保留清晰的资源存取接口 | ADR-0002 | 2026-10-19 |
| D10 | 当前 Task 2 范围 = **静态 Scene/Node、图片实例、Atlas/Region 元数据**；Spine 骨骼动画、约束、高级附件不进入本 Task | ADR-0002 + `Agent_Task/Task2.md` | 2026-10-19 |
| D11 | 导入器与求值器**只做独立实现**，不移植/翻译 Spine Runtime 源码 | [licenses-and-risks](docs/licenses-and-risks.md) §1.1.1 | 2026-10-19 |
| D12 | 回归**双口径**：Primary 963（对历史）/ Full 1113（形态覆盖）；报告须分别展示，**不得混用分母或改口径掩盖回归** | `tools/corpus-scan/` | 2026-10-19 |
| D13 | 能力标记三分法：**已验证支持 / 待验证 / 不支持**；不得以自造样本通过测试宣称真实兼容性验证通过 | `docs/mapping-spine32-to-native.md` §3.1 | 2026-10-19 |
| D14 | `spine38-web/` 与 `web38/` 标记为**不可分发**，不得进入公开发布包（许可证核查完成前） | [licenses-and-risks](docs/licenses-and-risks.md) | 2026-10-19 |
| D15 | **普通节点类型不限制父子关系**：GroupNode、ImageNode/SpriteNode 均可作为父节点和子节点；只有场景 Root 唯一、固定且不可移动/删除 | [场景系统规格](docs/scene-system/SCENE_SYSTEM_SPEC.md) §5；[Task 2](Agent_Task/Task2.md) | 2026-10-19 |
| D16 | 原生格式 v1 的 JSON 库 = **RapidJSON**，固定 release `v1.1.0`（commit `f54b0e47a08782a6131cc3d60f94d038fa6e0a51`，2016-08-25），**MIT**（已登记）。用其 **SAX 接口**自建保留未知字段的 JSON 值树并检测重复 key；**不直接用其 DOM**。**阶段 A 自研解析器继续作为探针，不是阶段 B 的正式解析器** | [RFC](docs/format/native-project-v1-rfc.md) §7.3–§7.5；[licenses](docs/licenses-and-risks.md) §1.5 | 2026-10-20 |
| D17 | 格式诊断码归 **Format 层**（`loongbones::format::FormatDiagnosticCode`），**不得**加入 Domain 的 `DiagnosticCode`；Domain 校验失败**保留既有** `DiagnosticCode` | [RFC](docs/format/native-project-v1-rfc.md) §9 | 2026-10-20 |
| D18 | 未知字段由 **Format 层文档对象**（草案 `ProjectDocument`）持有，Domain `ProjectData` **不新增**扩展数据成员；未知字段保留 JSON 值树，其中数值**逐字节保留原始字面量**，不经 `double` 舍入 | [RFC](docs/format/native-project-v1-rfc.md) §6.5–§6.6 | 2026-10-20 |
| D19 | `appVersion`：**写出时提供、读取时可省略、不参与任何格式兼容判定** | [RFC](docs/format/native-project-v1-rfc.md) §1.1、§5.1 | 2026-10-20 |
| D20 | 深度 64 / 文档 64 MiB / 字符串 1 MiB 是**可配置的 Reader 安全上限**，**不纳入持久化 Schema**；调整上限不产生新格式版本、不需要迁移 | [RFC](docs/format/native-project-v1-rfc.md) §4.4 | 2026-10-20 |
| D21 | **移除** `minimumReaderVersion <= formatVersion + 1` 约束（缺乏依据且人为限制演进） | [RFC](docs/format/native-project-v1-rfc.md) §1.1 | 2026-10-20 |
| D22 | 未知字段的宿主锚点 = **`(kind, 稳定 ID)`**（位置无关），编码时解析为当前 JSON Pointer；无 ID 容器用**最近的有 ID 祖先 + 相对子路径**；**禁止**用裸 JSON Pointer 作索引键（重排会挂错宿主）。锚点解析失败 = 宿主已删除 → 丢弃该记录 | [RFC](docs/format/native-project-v1-rfc.md) §6.5.1 | 2026-10-20 |
| D23 | **未知数值只允许有 `raw`，不做类型化转换**（故不存在越界）；**已知数值才按字段 schema 类型解析**（`integer`→`int64`，`number`→`double`），越界则**拒绝**并报 `kJsonNumberOutOfRange` | [RFC](docs/format/native-project-v1-rfc.md) §6.6、§9.3 | 2026-10-20 |
| D24 | 骨架作为核心可编辑数据采用**原生格式 v2**；v1 Reader 遇 v2 固定报 `FORMAT_VERSION_UNSUPPORTED`。含 `skeletons[]` 的 v2 必须声明 `loongbones.skeleton.v1` 为 required（能力拒绝与版本拒绝分别判定）；`minimumReaderVersion` 超限报 `FORMAT_TOO_NEW`。v1→v2 迁移须纯函数、保留未知字段、先迁移到内存；仅在用户保存时写出 v2。不得静默把骨架塞入 v1 可选字段或未知字段 | [Task 4 Back Task](Agent_Task/Task4_Back_task.md)；`docs/skeleton/skeleton-domain-rfc.md` §4.4 | 2026-10-20 |
| D25 | 持久化对象 ID **统一使用随机 ULID**；导入源 hash 仅作来源/重复提示，不参与 ID 派生。源稳定键仅供一次导入内引用映射；重复导入默认创建副本。未来显式更新/重导必须使用持久化 source-key 映射沿用现有 ID，不得由当前内容 hash 重算 | [ADR-0004](docs/adr/0004-stable-identity-strategy.md)；[Task 4 Back Task](Agent_Task/Task4_Back_task.md) | 2026-10-20 |
| D26 | 首个 Spine 导入插件采用**编译期静态注册**和抽象插件接口；不承诺动态库 ABI、运行时安装或崩溃隔离。Qt 编译器/ABI 定案后可另立决策改加载器 | `docs/skeleton/plugin-host-rfc.md`；[Task 4 Back Task](Agent_Task/Task4_Back_task.md) | 2026-10-20 |
| D27 | Task 4 只把 Skeleton 作为 Project 级资产导入，不放入 Scene。未来场景实例必须用独立的骨架实例节点/类型引用 Skeleton 资产；不得把 Bone 映射为 Scene Node，也不得伪装成普通 Sprite | `docs/skeleton/skeleton-domain-rfc.md`；[Task 4 Back Task](Agent_Task/Task4_Back_task.md) | 2026-10-20 |
| D28 | 不提供插件版本迁移钩子。插件版本用于来源记录与诊断；插件升级不自动改写已有 canonical 数据。重导/更新若实现，必须是用户显式操作并遵守稳定 ID 映射 | `docs/skeleton/plugin-host-rfc.md`；[Task 4 Back Task](Agent_Task/Task4_Back_task.md) | 2026-10-20 |
| D29 | 骨架第二批（mesh / 加权 mesh / linked mesh / IK / transform 约束）采用**直接增量修改结构 + 升 `formatVersion = 3`**，**不做 capability 协商层**。理由：能力门控只让旧 Reader「优雅报错」，并未解决「结构更新后解析器读不了」这一核心困境；当前无已发布存量工程与不可修改部件，大改现在可行。旧 v1 / v2 Reader 遇 v3 固定返回 `FORMAT_VERSION_UNSUPPORTED`；迁移链 v1→v2→v3 均为纯函数，写出统一为 v3 | [Task 5](Agent_Task/Task5.md) §3 A3、§9.1（Q5-1） | 2026-10-22 |
| D30 | 加权 mesh 的**权重保留源原值**，导入期只校验权重和合法（`sum = 0` / NaN / 越界即拒绝），**归一化由求值侧负责**。不得在导入期改写源语义 | [Task 5](Agent_Task/Task5.md) §9.2（Q5-2） | 2026-10-22 |
| D31 | `hull` / `edges` **不落 canonical**。决定性理由：本项目使用**独立 runtime**，且**不提供回写 / 回填 Spine 的方案**，因此无需为回写保真保留字段（也不必为可能不存在的回写能力预先实现 runtime 保真） | [Task 5](Agent_Task/Task5.md) §9.3（Q5-3） | 2026-10-22 |
| D32 | BoundingBox（语料 **0 例**）**保留类型占位，但不实现**：canonical / schema 保留类型位置与诊断码，不做解析、映射、校验与导入；支持矩阵写「不支持 + 待验证」，不得写「已验证支持」。有真实语料或功能需求时再实现；**校验 / 实现时机延后至 Qt GUI 阶段**（负责人 2026-10-22 裁定）**｜⚠️ 2026-10-24 由 D49 取代：BoundingBox 改为纳入 Task 7（非权重口径）**，本条仅作历史记录保留 | [Task 5](Agent_Task/Task5.md) §9.4（Q5-4）；取代：[Task 7](Agent_Task/Task7.md) §6 T7-D | 2026-10-22 |
| D33 | 共享图集布局（`h0164s5/watch/` 下 2 个单位共用与目录同名的 `.atlas.txt` → `IMPORT_RESOURCE_MISSING`）**不并入 Task 5**，另立 **`Agent_Task/Task6.md`**（给 `IResourceResolver` 增加目录列举能力） | [Task 5](Agent_Task/Task5.md) §9.5（Q5-5)；[Task 6](Agent_Task/Task6.md) | 2026-10-22 |
| D34 | 加权 mesh 的**单条负权重合法**，语义＝**该绑定对顶点无影响**；不拒绝、不诊断，原值保留（语料 86 例；fixture `07-weighted-mesh`）。**权重和的判据见 D37**（此处原写「和不含负值」已被 D37 收紧） | [Task 5 A1 裁定结果](Agent_Task/Task5_A1_Report.md) §5；[模型提案](docs/skeleton/mesh-and-constraint-rfc.md) §5 | 2026-10-22 |
| D35 | IK 约束除保留 `mix` 原值外，**新增布尔 `enabled`（`mix != 0`）记录是否开启**。理由：语料 `mix` 只有 0（302）与 1（2405），无任何中间值 | [Task 5 A1 裁定结果](Agent_Task/Task5_A1_Report.md) §5；[模型提案](docs/skeleton/mesh-and-constraint-rfc.md) §8 | 2026-10-22 |
| D36 | Transform 约束的 4 个 mix 与 6 个 offset **只保留源原值、不解释语义**（自洽方案）。理由：语料 mix 越出 [0,1]（`rotateMix` −180…142.6）且**从未取到 1**，与 Spine 3.5+ 的「0…1 插值因子」语义相悖，无证据不得解释。**求值器在 V3（动画时基）取证完成前不得读取这些字段做计算** | [Task 5 A1 裁定结果](Agent_Task/Task5_A1_Report.md) §5；[模型提案](docs/skeleton/mesh-and-constraint-rfc.md) §9 | 2026-10-22 |
| D37 | 加权 mesh 的**权重和必须有限且 > 0**：`sum = 0`、或 `sum < 0`、或 NaN / ±Inf 均判 `MESH_WEIGHT_SUM_INVALID` 并**拒绝该候选**。判据是**权重和**，不是单条绑定的符号 —— 单条负权重只要和仍 > 0 就合法（D34）。理由（负责人 2026-10-22）：「非法权重和为 0 即可，小于零也没有意义」。**不做**归一化、不把 0 当"均匀分摊" | [Task 5 A5 复核](Agent_Task/Task5_A5_Review.md)；[模型提案](docs/skeleton/mesh-and-constraint-rfc.md) §11；fixture `12-mesh-bad-weight-sum-zero` / `13-mesh-bad-weight-sum-negative` | 2026-10-22 |
| D38 | 合成 fixture **尽量对齐语料主流形态**，且**解析器**两条路径都要支持：① LinkedMesh —— 类型 4（父为非加权 mesh + 显式 `parentSkin`，**语料 0 例**，fixture `08`）+ 类型 5（父为加权 mesh + `parentSkin = null`，**语料 2 例**，fixture `14`）；② mesh 的 `nonessential` —— `=1` 仅 4/1113（fixture `06`）+ `=0` 占 1109/1113（fixture `16`）。理由（负责人 2026-10-22）：「最好对齐，不过不对齐也没问题，**解析器需要对齐**，我们用的（原生）格式没关系」⇒ 对齐是**解析器**的义务，canonical / 原生格式不受影响 | [Task 5 A5 复核](Agent_Task/Task5_A5_Review.md) §8；[fixtures README](docs/skeleton/fixtures/README.md) §1 | 2026-10-22 |
| D39 | **动画时基 / 帧率：默认 60 FPS**。存储统一用整数 tick / 整数帧位置（规范 §4 既定），转换采用有理数 / 明确舍入规则（规范 §4 既定）；当且仅当真实样本证明非 60 时再调整。**此裁定解除 V3 对动画模块与 D36 transform mix/offset 求值的时基阻塞**（D36「保留原值、不解释语义」的约束不变；mix/offset 的语义解释仍需真实样本证据，见 D36） | 负责人 2026-10-22 裁定 | 2026-10-22 |
| D40 | 共享图集（Task 6 Q6-1）：同一份 `.atlas.txt` 被多个骨架引用时，canonical 内**复用同一个 Atlas / Texture 资产**（指向同一份文件）；具体对象**可选共用或独立化**。与 D25「重复导入默认创建副本」不冲突（D25 针对同一单位重复导入，本场景是同一次导入内多单位共享同一资源） | `Agent_Task/Task6.md` Q6-1；负责人 2026-10-22 裁定 | 2026-10-22 |
| D41 | Spine 兼容边界：本项目**仅逆向理解 Spine 拥有的数据类型**（字段/语义），**不涉及 Spine Runtime 源码挪用、实现挪用，也不做分发**；法律上无风险。发布前仍按 Q4 就「公式等价重写 + 格式解析」取得 Esoteric 书面确认作为独立审查事项 | 负责人 2026-10-22 裁定；[licenses-and-risks](docs/licenses-and-risks.md) §1.1.1 / §3 | 2026-10-22 |
| D42 | 稳定顶点 ID / 索引契约的**工程零点参考 = (0,0,0)**；网格顶点 / 索引以该原点为基准（Phase 4 deform 与顶点编辑前置，待 Phase 3 之后落地） | 负责人 2026-10-22 裁定 | 2026-10-22 |
| D43 | **渲染 / 视觉实现延后，但渲染器与图形库的选型必须在 Task 7 阶段 A 定案**（方案先定、实现后做）。视口渲染属 GUI 层，经既有 **Render Adapter** 隔离，界面框架不得渗入 Domain / Runtime；浮点 / 角度 / 坐标系 / 混合模式等须为 schema 级书面契约。**负责人补充约束（2026-10-24）**：① **Qt 版本不绑定** ⇒ 不得选仅 Qt 6 可用的后端（**QRhi 出局**）；② **界面可能继续优化** ⇒ 后端必须可替换、UI 改动不得牵连渲染层；③ 平台优先级 **Windows > macOS > Linux**，性能基线＝**AMD 核显流畅即可**（见 D48）。**推荐：QOpenGLWidget 承载 + 自研 OpenGL core（Win/Linux 3.3、macOS 4.1）2D 渲染器**（Qt 5/6 通用、无第三方渲染依赖、可精确表达带 UV 网格与混合模式；代价是 macOS 上 OpenGL 已被 Apple 弃用，靠 Render Adapter 预留替换）；**备选 bgfx**（一套代码拿 D3D/Metal/Vulkan/GL，消除 macOS 风险，代价是第三方依赖）；**排除 QPainter / QGraphicsView**（画不了带 UV 的纹理网格，而 mesh 是核心资产）。**QML 已裁定采用**（见 **D50**，含义与权衡见 Task 7 §5.3）。最终由负责人签定 ADR | 负责人 2026-10-24 裁定；[Task 7](Agent_Task/Task7.md) §2.1 / §5、Q7-1；QML 见 D50 | 2026-10-24 |
| D44 | 时间线取值方案**完全扩展**：除 stepped / linear 外，**连续与曲线（bezier，含控制手柄）**取值，以及**额外手柄控制函数带来的复杂行为**都要支持，**不得只做「原值承载」**。曲线类型、控制点 / 手柄存储形态与求值公式由 Task 7 阶段 A 取证后落定 | 负责人 2026-10-24 裁定；[Task 7](Agent_Task/Task7.md) §2.2、Q7-2 | 2026-10-24 |
| D45 | **绑定（蒙皮）只做「不负责权重」的部分** —— 骨骼变换层级与**子物体矩阵**（child object matrices）的求值与传播可以做；**权重的显示等权重相关能力延后**。与 D30（权重保留源原值、归一化归求值侧）不冲突 | 负责人 2026-10-24 裁定；[Task 7](Agent_Task/Task7.md) §2.3、Q7-1 | 2026-10-24 |
| D46 | **原生格式升级为 `formatVersion = 4`**（承载时间线 / 动画 / 事件等），沿用 **D29**「直接增量改结构 + 升版本、不做 capability 协商层」模式；迁移链 **v3 → v4** 为**纯函数**（不生成 ID、不改既有字段、不碰磁盘、幂等），写出统一为 v4；v1/v2/v3 Reader 对 v4 固定拒绝、不降级、不静默丢字段 | 负责人 2026-10-24 裁定；[Task 7](Agent_Task/Task7.md) §4、Q7-4 | 2026-10-24 |
| D47 | **先确定「时间平滑程度」等基础问题再铺开实现**：时间粒度 / 采样 / 插值基准，以及 **D39**（默认 60 FPS、整数 tick）与曲线连续取值如何衔接，必须由 Task 7 阶段 A 以语料证据定清；**定清前不得铺开实现**（否则大面积返工） | 负责人 2026-10-24 裁定；[Task 7](Agent_Task/Task7.md) §4、Q7-5 | 2026-10-24 |
| D48 | **目标平台优先级与性能基线**：**Windows > macOS > Linux**；**性能基线＝在 AMD 核显（集显）上可流畅使用即可**。2D 骨架渲染负载不高 ⇒ 选型重**三平台兼容面与驱动稳定性**，不追求峰值性能；据此 macOS 需认真对待 OpenGL 弃用问题（Metal 后端或 Render Adapter 预留替换） | 负责人 2026-10-24 裁定；[Task 7](Agent_Task/Task7.md) §5.1 / §5.2 | 2026-10-24 |
| D49 | **`BoundingBox` 纳入 Task 7**（Q7-3 已裁定），按**非权重口径**实现（与 D45 一致）：canonical / schema / 映射 / 校验 / 导入与 v4 落盘都做，但**权重相关（加权 boundingbox、权重显示）不纳入**。**本条取代 D32「延后至 Qt GUI 阶段」**。**仍受 D13 约束**：语料 **0 例**（Full 口径）⇒ 只能用合成 fixture 证明「可解析 / 可映射 / 可校验」，**不得宣称「语料已验证」**，支持矩阵须写「已实现 + 待真实样本验证」 | 负责人 2026-10-24 裁定；[Task 7](Agent_Task/Task7.md) §6 T7-D、Q7-3 | 2026-10-24 |
| D50 | **采用 QML（Qt Quick）作为界面承载**（时间轴 / 曲线编辑器 / 属性面板 / 视口叠加层等）。**视口不由 QML 的图形栈决定**：渲染仍经 **Render Adapter** 隔离，推荐以 **`QQuickFramebufferObject`**（Qt 5 / Qt 6 均有 ⇒ 不绑 Qt 版本）把自研渲染器接入 QML 场景，渲染后端一期仍是自研 **OpenGL core**。⇒ QML 的采用只改变**界面写法与视口承载方式**，**不改变「用什么图形 API 画网格」**；与「界面可能继续优化」叠加时，UI 更替不得牵连渲染层 | 负责人 2026-10-24 裁定；[Task 7](Agent_Task/Task7.md) §5.3 / §5.5 | 2026-10-24 |
| D51 | **接受引入第三方渲染库（bgfx），但作为「后期单独发行版本」，不作为一期的构建硬依赖**：一期 = **自研 OpenGL core 渲染器**（零第三方渲染依赖）；后期发行一个 **bgfx 后端的独立变体 / 构建开关**，一次拿到 D3D / Metal / Vulkan / GL，消除 macOS OpenGL 弃用风险。⇒ 硬要求是 **Render Adapter 必须支持多后端并存与构建期选择**，不得让一期代码被任一后端绑死 | 负责人 2026-10-24 裁定；[Task 7](Agent_Task/Task7.md) §5.2 / §5.5 | 2026-10-24 |
| D52 | **最低硬件 / 驱动基线：尽可能支持**（尽量下探兼容下限，不追新特性）。一期 OpenGL 后端按 **GL 3.3 core（Windows / Linux）、GL 4.1（macOS）** 为基线，**不依赖 GL 4.x 特性与厂商扩展**；达不到基线时须给出**明确诊断并拒绝/降级**，不得静默崩溃或黑屏。后期 bgfx 变体（D51）进一步拓宽到 Metal / D3D / Vulkan。与 D48（AMD 核显流畅即可）互补：D48 定**目标体验**，本条定**兼容下限** | 负责人 2026-10-24 裁定；[Task 7](Agent_Task/Task7.md) §5.4 / §5.5 | 2026-10-24 |
| D53 | **共享图集在 canonical 中共用同一份 `Atlas` / `TextureAsset` 对象**（不是每个骨架各建一份副本）：同一份源文件只对应**一个** `Atlas.id` 与**一个** `TextureAsset.id`，多个骨架的 `atlasIds` 指向同一个。与 **D40**（复用同一份资产）一致；与 **D25**（同一单位重复导入默认建副本）**不冲突** —— D25 的主体是「同一单位重复导入」，本场景是「两个不同单位引用同一份源资产」，语义不同、非其例外。**阶段 B 必须验证**：`atlases` / `textures` 各 1 份、`draft.resources` 对同一 PNG 不重复、重开后仍 1 份；**同一源文件被两个骨架引用时不得各自新建 `Atlas.id`**（对象 ID 唯一 ≠ 源文件路径相同） | 负责人 2026-10-24 认可（✅「认可」）；A1/A3 取证 `docs/skeleton/shared-atlas-layout-evidence.md` §6。**⚠️ 落地时机见 D55：Task 6 内不实现（v3 已冻结、缺源身份字段）⇒ Task 7 / v4** | 2026-10-24 |
| D54 | **Task 6 图集匹配策略**（Q6-2 / Q6-3 裁定）：① 优先级＝**源内显式引用 → 骨架同名 → 目录内唯一候选**；② **歧义（目录内 ≥2 个且无同名）必须显式拒绝 + 可定位诊断**（含候选数），**不得猜测、不得取第一个**（负责人裁定「**歧义拒绝**」）；③ **不实现上级目录回退**（解析路径 0 例，但父目录确有候选：138 个骨架目录 / 103 个不同父目录 / 145 个单位，其中 **107 个**为父目录唯一候选 ⇒ 实现即误吸风险）；④ **不变量：「骨架同名」必须排在「目录内唯一候选」之前** —— 否则多图集目录里的 **11 个**单位会静默解析到错图集。**语料事实双口径**：多候选目录**有** 5 个 / 11 张图集，但**真歧义 0 例**（全被同名消解）⇒ 歧义判据只作**防御性**，不得宣称支持歧义布局（D13） | 负责人 2026-10-24 裁定；复核 `Agent_Task/Task6_A1A3_Review.md` §4；取证 §4 | 2026-10-24 |
| D55 | **D53 的落地时机＝Task 7（v4），Task 6 内不实现「跨单位共用同一 Atlas / Texture 对象」**。原因：判定「同一份源文件」需要**源文件身份**，而 `Atlas`（`atlas.hpp:106-111`）与 `TextureAsset`（`:54-63`）**都没有源路径字段**；按 `name` 去重**不安全** —— 语料 **33 个图集 stem 跨目录重名**（如 `b0037s3` 出现在 4 个目录）⇒ 会误合并不同图集；而 **v3 已在 Task 5 冻结**（D29 模式），新增字段属 **v4**（D46）。⇒ **Task 6 必须明写已知限制**：两个共享单位导入同一工程时**各建一份 Atlas / Texture**（同一 PNG 写两份字节）；**不得**把该形态表述为「资产共用已实现」。**Task 7 落地时的连带改动**：v4 增源身份字段 → 按源路径复用 → 同步改造 `tools/skeleton-import-smoke/src/main.cpp` 中 `atlases.size() == committed` 的「1 单位 = 1 图集」断言（改为按去重后的源图集数） | 负责人 2026-10-24 裁定「A」；复核 `Agent_Task/Task6_B_Review.md` §7.1 | 2026-10-24 |

---

## 2. 待验证（必须有真实证据 + 可复现程序，不得凭经验假设）

| # | 事项 | 为什么不能拍板 | 验证要求 | 当前阻塞谁 |
|---|---|---|---|---|
| V1 | **UV 的 V 方向** | Spine/OpenGL 惯例 V 向上，图像/2D 管线 V 向下；取错导致贴图上下翻转，且对称图上看不出来 | 真实样本 + 可复现程序，输出数值而非"看起来对"；**禁用旧 Canvas 解释器产出作判据** | 动画/渲染模块 |
| V2 | **图集缩放承载层**（图集层 vs 附件层） | Spine `regionScale = width/originalWidth * scaleX` 与 DragonBones 图集层 `scale`（取倒数）**不等价**，rotation≠0 时结果不同 | 分别验证：未旋转 / 90° 旋转 / 裁切 / 原始尺寸≠图集尺寸 / 非等比缩放；区分原始图像尺寸、裁切区域、旋转状态、附件缩放、图集层级变换。**不得仅凭字段名或公式相似判等价**。→ **2026-10-20 部分解决**：承载层已定为**图集页层**（region 坐标属页头声明 `size` 空间，PNG 为更高分辨率同一图，缩放 `pixel/page`），像素判据 26 支持 / 0 反对 / 3 不确定；**rotation≠0 的复合仍待验证（T-A1）**。见 `docs/skeleton/skeleton-domain-rfc.md` §6.7 | 导入 + 渲染模块 |
| ~~V3~~ | ~~动画时基 / 帧率 / 舍入规则~~ | ✅ **2026-10-22 已定案 D39**：默认 60 FPS；存储用整数 tick/帧、舍入用有理数/明确规则（规范既定）；当且仅当真实样本证明非 60 时再调整。**已解除对动画模块与 D36 求值的时基阻塞**（D36「保留原值、不解释语义」的约束不变） | 已解除（见 D39） |
| ~~V4~~ | ~~稳定 ID 策略~~ | ✅ 2026-10-20 定案：随机 ULID；导入默认创建副本，source hash 不派生 ID。见 D25/ADR-0004 | 已用于 Project/Node；骨架导入按同一策略 |
| V5 | Qt 实际使用的编译器（MinGW 还是 MSVC） | 影响核心库 ABI 一致性；见 ADR-0001「ABI 风险」 | Qt 装完后确认；若为 MSVC 则核心需改用 MSVC 重建，或改 pImpl/C ABI | 视觉模块（Task 1 不阻塞） |
| ~~V6~~ | ~~名称唯一性统计（bone/slot/附件名）~~ | ~~决定导入时的稳定键选取~~ | ✅ **2026-10-20 已解决**：骨骼重名 **0**、插槽重名 **0**、同 slot 内附件重名 **0**（1113/1113）→ 名称可作稳定键，重名时按 `<name>#<序号>` 消歧并告警。见 `docs/skeleton/skeleton-domain-rfc.md` §7.2 | 导入模块 |
| ~~V7~~ | ~~`nonessential` 段覆盖率与语义~~ | ~~影响"哪些字段可安全丢弃"~~ | ✅ **2026-10-20 已解决**：`nonessential` true **4** / false **1109**；`imagesPath` 非空 **4** → 该段几乎不存在，可安全列为"不支持且有诊断"。见 `docs/skeleton/field-mapping.md` §1 | 导入模块 |
| ~~V8~~ | ~~`linked mesh` 出现率~~、~~`deform offset` 形态分布~~ | ~~影响 schema 是否需要这些字段~~ | 🟡 **2026-10-20 部分解决**：linked mesh **2** 例（Full/primary 均为 2）；deform 时间线 **335,817**（Full）→ schema 需 `parentName` 字段，但**无真实语料可往返验证**；`deform offset` 形态分布**仍未统计**（动画首批不支持，非阻塞） | 动画模块 |

---

## 3. 当前阶段不涉及（但结论不得遗忘）

| # | 事项 | 已确认的结论（进入对应模块时直接采用） |
|---|---|---|
| N1 | 骨骼动画帧值域 | Spine 骨骼帧是**相对 setup 的增量**，非绝对值（照绝对值减 setup 会让 `mecha/idle/root` 差 90°） |
| N2 | 非加权 mesh 顶点空间 | Spine 是**槽骨骼局部坐标**（DragonBones 是骨架空间绝对坐标；弄错会"动画一播就散"）。**2026-10-22 Task 5 A1 对照实验支持该结论**：加权顶点每条 `(bone, x, y)` 是**对应骨骼的局部坐标**（各条目变换到骨架空间后重合，mean 5.5 vs 原始 122.1）；中途曾因判据无效被误判为「骨架空间」，已撤回，见 `Agent_Task/Task5_A1_Review.md` §3 P1 |
| N3 | mesh UV 的空间 | Spine 文件里的 `uvs` 就是 **regionUVs（region 相对 [0,1]）**；整页 UV 由 runtime `UpdateUVs()` 加载时算。当整页坐标再逆变换 = 双重变换，UV 被拉出 [0,1] |
| N4 | 曲线形态 | 线性 / stepped / 贝塞尔(4 点) 确定；"采样点"形态出现率待统计 |

---

## 4. 待你确认（阻塞对应工作）

| # | 问题 | 我的建议 | 阻塞 |
|---|---|---|---|
| ~~Q1~~ | ~~ADR-0004 ID 策略~~ | ✅ **随机 ID（A）**，不做内容 hash 派生；稳定键仅做导入内引用映射 | 已定案 D25 |
| ~~Q2~~ | ~~ID 形式~~ | ✅ **ULID** | 已定案 D25 |
| ~~Q3~~ | ~~重复导入默认策略~~ | ✅ **创建副本**；链接/更新已有对象须显式操作 | 已定案 D25 |
| Q4 | 正式发布前是否需就"数学公式等价重写 + 数据格式解析"取得 Esoteric Software 书面确认？ | 当前阶段不做法律判断，但**发布前须作为独立审查事项**；待核查问题清单见 licenses §3 | 正式发布（非当前） |
| Q5 | Task 6 的 Q6-2（目录内多候选图集如何处置）/ Q6-3（是否支持图集在上级目录等其它布局）| 负责人 2026-10-22：**问题不明确**，不预先裁定；待 Task 6 阶段 A 语料取证确认是否真有此类样本后再定。**2026-10-24 A1 取证已就位**（`docs/skeleton/shared-atlas-layout-evidence.md`，本对话独立复跑逐项吻合；复核 `Agent_Task/Task6_A1A3_Review.md`）：语料**有** 5 个目录含多张图集（11 张）、**有** 138 个骨架目录的父目录含候选图集（103 个不同父目录 / 涉 145 个单位，其中 107 个为父目录唯一候选）；但**实际解析路径**上多候选 0 例、上级目录 0 例。**✅ 2026-10-24 已裁定 → `D54`（负责人「歧义拒绝」）**：Q6-2 = 歧义**显式拒绝 + 可定位诊断**；Q6-3 = **不实现**上级目录回退；不变量「骨架同名优先于目录内唯一候选」；A3 资产语义 → **D53**。取证：`docs/skeleton/shared-atlas-layout-evidence.md`，复核 `Agent_Task/Task6_A1A3_Review.md` | **已关闭**（D54 / D53） |

---

## 5. 语料能力边界（决定"待验证"项的规模）

来自 Phase 0 实测（primary 963）：

| 能力 | 实证 | 标记 |
|---|---|---|
| bone / slot / region | 65,171 / 38,821 / 24,605 | 可验证 |
| mesh（非加权 + 加权） | 18,325 / 15,751 | 可验证 |
| deform（FFD） | 316,722 条时间线 | 可验证 |
| IK | 2,481 约束 + 36,408 时间线 | 可验证 |
| transform 约束 | 160 | 可验证（样本少） |
| drawOrder | 14,136 | 可验证 |
| **event** | **1 个 event、0 条时间线** | 待验证 |
| **path 约束** | **0** | 待验证 |
| **boundingbox / clipping / point / path 附件** | **0** | 待验证 |
| **sequence 附件** | `index` 全 -1 | 待验证 |

以上"待验证"项**均不阻塞当前资产组合器**（负责人 2026-10-19 确认）。
若后续 Spine 兼容目标确实要求，须分别列出**样本缺口 / 实现状态 / 验证计划**，
不得以自造样本通过测试就宣称真实兼容性验证通过。
