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
| D32 | BoundingBox（语料 **0 例**）**保留类型占位，但不实现**：canonical / schema 保留类型位置与诊断码，不做解析、映射、校验与导入；支持矩阵写「不支持 + 待验证」，不得写「已验证支持」。有真实语料或功能需求时再实现 | [Task 5](Agent_Task/Task5.md) §9.4（Q5-4） | 2026-10-22 |
| D33 | 共享图集布局（`h0164s5/watch/` 下 2 个单位共用与目录同名的 `.atlas.txt` → `IMPORT_RESOURCE_MISSING`）**不并入 Task 5**，另立 **`Agent_Task/Task6.md`**（给 `IResourceResolver` 增加目录列举能力） | [Task 5](Agent_Task/Task5.md) §9.5（Q5-5)；[Task 6](Agent_Task/Task6.md) | 2026-10-22 |
| D34 | 加权 mesh 的**单条负权重合法**，语义＝**该绑定对顶点无影响**；不拒绝、不诊断，原值保留（语料 86 例；fixture `07-weighted-mesh`）。**权重和的判据见 D37**（此处原写「和不含负值」已被 D37 收紧） | [Task 5 A1 裁定结果](Agent_Task/Task5_A1_Report.md) §5；[模型提案](docs/skeleton/mesh-and-constraint-rfc.md) §5 | 2026-10-22 |
| D35 | IK 约束除保留 `mix` 原值外，**新增布尔 `enabled`（`mix != 0`）记录是否开启**。理由：语料 `mix` 只有 0（302）与 1（2405），无任何中间值 | [Task 5 A1 裁定结果](Agent_Task/Task5_A1_Report.md) §5；[模型提案](docs/skeleton/mesh-and-constraint-rfc.md) §8 | 2026-10-22 |
| D36 | Transform 约束的 4 个 mix 与 6 个 offset **只保留源原值、不解释语义**（自洽方案）。理由：语料 mix 越出 [0,1]（`rotateMix` −180…142.6）且**从未取到 1**，与 Spine 3.5+ 的「0…1 插值因子」语义相悖，无证据不得解释。**求值器在 V3（动画时基）取证完成前不得读取这些字段做计算** | [Task 5 A1 裁定结果](Agent_Task/Task5_A1_Report.md) §5；[模型提案](docs/skeleton/mesh-and-constraint-rfc.md) §9 | 2026-10-22 |
| D37 | 加权 mesh 的**权重和必须有限且 > 0**：`sum = 0`、或 `sum < 0`、或 NaN / ±Inf 均判 `MESH_WEIGHT_SUM_INVALID` 并**拒绝该候选**。判据是**权重和**，不是单条绑定的符号 —— 单条负权重只要和仍 > 0 就合法（D34）。理由（负责人 2026-10-22）：「非法权重和为 0 即可，小于零也没有意义」。**不做**归一化、不把 0 当"均匀分摊" | [Task 5 A5 复核](Agent_Task/Task5_A5_Review.md)；[模型提案](docs/skeleton/mesh-and-constraint-rfc.md) §11；fixture `12-mesh-bad-weight-sum-zero` / `13-mesh-bad-weight-sum-negative` | 2026-10-22 |
| D38 | 合成 fixture **尽量对齐语料主流形态**，且**解析器**两条路径都要支持：① LinkedMesh —— 类型 4（父为非加权 mesh + 显式 `parentSkin`，**语料 0 例**，fixture `08`）+ 类型 5（父为加权 mesh + `parentSkin = null`，**语料 2 例**，fixture `14`）；② mesh 的 `nonessential` —— `=1` 仅 4/1113（fixture `06`）+ `=0` 占 1109/1113（fixture `16`）。理由（负责人 2026-10-22）：「最好对齐，不过不对齐也没问题，**解析器需要对齐**，我们用的（原生）格式没关系」⇒ 对齐是**解析器**的义务，canonical / 原生格式不受影响 | [Task 5 A5 复核](Agent_Task/Task5_A5_Review.md) §8；[fixtures README](docs/skeleton/fixtures/README.md) §1 | 2026-10-22 |

---

## 2. 待验证（必须有真实证据 + 可复现程序，不得凭经验假设）

| # | 事项 | 为什么不能拍板 | 验证要求 | 当前阻塞谁 |
|---|---|---|---|---|
| V1 | **UV 的 V 方向** | Spine/OpenGL 惯例 V 向上，图像/2D 管线 V 向下；取错导致贴图上下翻转，且对称图上看不出来 | 真实样本 + 可复现程序，输出数值而非"看起来对"；**禁用旧 Canvas 解释器产出作判据** | 动画/渲染模块 |
| V2 | **图集缩放承载层**（图集层 vs 附件层） | Spine `regionScale = width/originalWidth * scaleX` 与 DragonBones 图集层 `scale`（取倒数）**不等价**，rotation≠0 时结果不同 | 分别验证：未旋转 / 90° 旋转 / 裁切 / 原始尺寸≠图集尺寸 / 非等比缩放；区分原始图像尺寸、裁切区域、旋转状态、附件缩放、图集层级变换。**不得仅凭字段名或公式相似判等价**。→ **2026-10-20 部分解决**：承载层已定为**图集页层**（region 坐标属页头声明 `size` 空间，PNG 为更高分辨率同一图，缩放 `pixel/page`），像素判据 26 支持 / 0 反对 / 3 不确定；**rotation≠0 的复合仍待验证（T-A1）**。见 `docs/skeleton/skeleton-domain-rfc.md` §6.7 | 导入 + 渲染模块 |
| V3 | 动画时基 / 帧率 / 舍入规则 | Spine 3.2.01 二进制头**无 fps 字段**；旧项目写死 30 是**未经验证的假设** | 依真实样本确定；**不得沿用固定 30 FPS** | 动画模块（D3 未定，非当前阻塞） |
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
