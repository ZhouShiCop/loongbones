# Task 4 Back Task：阶段 A 文档契约返修

- 状态：**复核通过，已关闭**；阶段 A 已批准，阶段 B 未获批准、未启动。
- 性质：只修改文档与决策台账；不改代码、不跑实现、不访问或复制游戏资产。
- 依据：负责人对阶段 A 的复核裁定；具体文件以本清单为准。
- 返修结果：第一轮见 `docs/skeleton/ACCEPTANCE.md` §7；第二轮（§4）见同文件 §8；**第三轮（§5）见同文件 §9**（逐项发现 → 返修落点）。

## 1. 已裁定事项（必须同步到所有对应文档）

1. **原生格式**：采用 v2 承载 Skeleton。v1 Reader 明确拒绝 v2；v1→v2 是纯函数迁移，未知字段保留，迁移结果先在内存中使用，只有用户保存时才写出 v2。禁止把骨架作为 v1 可选字段或未知字段透传。
2. **ID/重复导入**：持久化对象一律由宿主分配随机 ULID。源内容 hash 只用于来源元数据/重复提示，不参与 ID 派生。稳定键只建立单次导入中的引用映射；重复导入默认创建副本。未来显式更新/重导须通过持久化 source-key→ID 映射沿用 ID。
3. **插件加载**：首个 Spine 插件用编译期静态注册，宿主只依赖抽象接口；不承诺动态库 ABI、运行时安装或崩溃隔离。未来加载器变化另行裁定。
4. **Scene 放置**：Task 4 导入的 Skeleton 是 Project 级资产，不创建 Scene 实例。后续实例须由专用骨架实例类型引用 Skeleton；Bone 不映射为 Scene Node，也不伪装为 Sprite。
5. **插件版本**：不提供插件版本迁移钩子。插件版本仅作来源记录和诊断；升级不自动改写 canonical 数据。更新/重导须由用户显式触发。

## 2. 必须修订项

### 2.1 骨架 RFC 结构与格式

- `docs/skeleton/skeleton-domain-rfc.md` §3.1：移除与 Skeleton 并列的顶层 `skins[]`。Skin 属于 Skeleton，关系须与 §1.2、§5.4 一致。
- §4：删除“选项 A 仅因 required capability 才安全”的自相矛盾论证。明确 v1 可选未知字段的读写风险、required capability 的拒绝语义，并将已裁定 v2 方案写为唯一采用方案；保留方案比较时标为历史备选，不再保留裁定请求。
- §4 的迁移契约须补足版本字段、minimumReaderVersion、能力字段与保存时机；与 Task 3 codec 能力不一致之处列为阶段 B 的前置实现任务，不宣称已有 v2 Reader/Writer。
- §7：按随机 ULID 更新 ID 方案。明确 source hash 不是 ID 输入，稳定键只用于导入内引用；重复导入创建新对象。去重提示不能隐式复用 ID。
- §5/§10：明确 RFC 提案的模型覆盖与 Task 4 当前垂直切片范围是两件事。当前阶段 B 只实现骨架元数据、Bone、Slot、Skin、RegionAttachment、Atlas/Region 映射；mesh/weighted/linked mesh、IK/transform 参数、动画/deform 不得标作本 Task “首批实现/已支持”，须归入后续任务或仅定义未来模型提案。
- §9：增加源二进制非有限浮点处理规则与稳定诊断码；NaN/Inf 或解析后非有限值必须拒绝整个候选。fixture 和阶段 B 验收覆盖非有限 float、字节偏移及失败原子性。
- §3.2、Q-A1：把已裁定的“仅导入资产、不做 Scene 放置；未来独立实例类型引用 Skeleton”写成决定，不再列为待裁定。
- 删除已裁定项 R-A1、R-A2、Q-A1、Q-C1 等待决表述；保留真正未决/待验证项目并明确负责人/阶段。

### 2.2 插件 RFC

- `docs/skeleton/plugin-host-rfc.md` §3：把静态注册改为已定案，不再要求负责人选择加载形态；明确本阶段不具运行时发现/安装、ABI 稳定或进程隔离承诺。
- §7.3：更新为宿主统一随机 ULID 分配；移除“确定性派生策略唯一实现点”的说法。
- §8：移除插件版本升级迁移钩子的待决项，落入“不自动迁移；显式重导/更新”的规则。
- 检查输出模型、原子提交、sourceRef 与宿主接口是否与随机 ID、默认创建副本、Task 4 静态切片一致。

### 2.3 证据、范围与索引一致性

- `docs/skeleton/field-mapping.md`：区分“解析/证据映射”“未来 canonical 模型提案”“Task 4 实际实现范围”。将 mesh/weighted/linked mesh 与 IK/transform 从本任务首批支持措辞中移出。
- `docs/skeleton/sample-manifest.md`：为 16 个样本补充 atlas 与伴随 PNG 的 SHA-256（以及字节数），或明确本 manifest 的 hash 只锁定 skeleton 输入，并新增可复现的 companion manifest；所有路径仍相对 corpus root，不写绝对路径、不复制文件。
- `docs/skeleton/ACCEPTANCE.md`：探针脚本数量应与现有目录一致（4 个 `.mjs`：probe、PNG 判据、fixture 生成、样本导出）；报告内容/日期保留为真实运行记录。增加本次复核发现及返修结果。
- `docs/skeleton/README.md`、`Agent_Task/Task4.md`、`STRUCTURE.md`：移除已经裁定的待决列表；新增 Back Task 索引；状态统一为“阶段 A 待返修，阶段 B 未获批准”。
- `DECISIONS.md` 与 ADR-0004：同步 D24–D28 的裁定；ADR-0004 的 ID 策略状态由草案更新为已定案，Q1/Q2/Q3 逐项关闭；保留 Q4 发布前合规审查。

## 3. 验收标准

- 全仓搜索不再出现把已裁定事项标作“待负责人裁定/待确认”的旧文字；同一规则在 Task、RFC、插件契约、README、决策台账中一致。
- Skin 所有权只有一种表达；Project 顶层不出现无归属的 `skins[]`。
- Task 4 阶段 B 范围与支持矩阵一致，不把未来能力写成首批实现。
- v2 迁移/旧 Reader 拒绝语义、随机 ULID 与副本导入、静态插件加载、无自动插件迁移、Skeleton 不入 Scene 的决定可从文档独立理解。
- 验收报告明确列出语料盲区与未经真实语料验证的行为；不以合成 fixture 宣称实测兼容。
- 只改文档与索引，不启动阶段 B，不新增/修改产品代码；提交返修完成清单，等待负责人复核。

## 4. 复核追加项（2026-10-20）

第二轮静态复核发现以下契约仍有歧义/缺项，完成后重新提交复核：

1. **骨架能力标识**：`skeleton-domain-rfc.md` §4.4/§4.5 不得把骨架能力 `required` 还是 `optional` 留待阶段 B 决定。裁定：有 `skeletons[]` 的 v2 文档必须声明 `loongbones.skeleton.v1` 为 required；不认识该能力的 Reader 必须拒绝。该能力不替代 v2 版本检查。
2. **稳定格式错误码**：§4.1 移除 `FORMAT_VERSION_UNSUPPORTED` / `FORMAT_TOO_NEW` 二选一。旧 v1 Reader 遇 `formatVersion=2` 固定返回现有稳定码 `FORMAT_VERSION_UNSUPPORTED`；`minimumReaderVersion` 超出当前 Reader 上限继续使用对应的 reader-version 错误码。
3. **三件套可定位**：`sample-manifest.md` 除 SHA-256 与字节数外，必须分别列出 `.atlas.txt` 和 `.png` 相对语料根的路径。每个样本明确其关联纹理；若一个 atlas 引用多个 PNG，全部列出并明确页名/顺序；缺失文件写 `—`。不能仅凭 hash 猜具体文件。
4. **哈希字段名统一**：`field-mapping.md` 头部 `hash` 不得映射为 `sourceRef.sourceHash`；与领域 RFC、插件契约统一命名为 `sourceRef.contentHash`，并注明算法及覆盖的源文件范围。若 atlas/PNG hash 也保存，需区分源骨架 hash 与伴随文件摘要，不能混称一个 `contentHash`。
5. **支持矩阵分类**：`skeleton-domain-rfc.md` §10.2 标题“仅解析保留 / 不支持且有诊断”与正文“工程内不留任何等价物”冲突。统一为“不支持且有诊断（不保存源片段）”，除非明确定义并实现一个不含源序列化的解析统计字段。
6. **验收记录准确性**：`ACCEPTANCE.md` §7.6 “全仓搜索无待决项”不得把 Back Task 历史文字里的“待负责人裁定”匹配也当作现行状态；区分历史复核记录与规范当前状态。保持 §1–§6 为历史运行记录即可，无需重写整份报告；新增 §8 记录本次追加发现与返修结果。

### 4.1 复核验收

- v2 版本拒绝、required capability 拒绝各自有单一明确规则与稳定错误码，且不互相替代。
- 16 个 manifest 样本的 atlas/PNG 路径与 hash 对应唯一；多纹理情况无遗漏。
- `contentHash` 命名及 hash 覆盖范围一致且可复现。
- §10.2 分类不暗示保存源字节或保留不支持语义。
- RFC、manifest、ACCEPTANCE、Task、README、STRUCTURE 当前状态一致；阶段 B 仍未获批准。

## 5. 第三轮复核追加项（2026-10-20）

1. **源头 `hash` 与计算摘要不可混为一字段**：`field-mapping.md` §1 将 Spine 二进制头内的 `hash` 原样映射至 `sourceRef.contentHash`，但同一行又把 `contentHash` 定义成导入器对整个 `.skel.bytes` 计算的 SHA-256。两者来源和算法不同。请将 Spine 头字段改为独立的可选来源元数据（例如 `sourceHeaderHash`；如不进入本任务 canonical 模型则映射为“不持久化，仅报告”），`sourceRef.contentHash` 只由宿主/插件对源文件全文计算；样本 manifest 的 SHA-256 仍作为外部复现锁定值。同步 RFC、field-mapping、插件返回结构和验收报告，禁止写“原样”映射。
2. **状态文字统一**：第二轮返修已提交后，`STRUCTURE.md` 仍称“复核追加返修中”，而 Task/README/Back Task 称“两轮返修完成，待复核”。在本项完成后把所有现行状态统一为“阶段 A 返修完成，待复核；阶段 B 未获批准、未启动”。历史记录中的阶段状态不要求改写。

### 5.1 复核验收

- Spine 头部源 `hash` 与文件 SHA-256 `contentHash` 有不同名称、来源、算法和去向；无字段误映射。
- 插件结果、RFC、字段表、manifest 对两种 hash 的定义一致。
- `Task4.md`、Back Task、`STRUCTURE.md`、`docs/skeleton/README.md` 的当前状态一致；阶段 B 仍未获批准。
