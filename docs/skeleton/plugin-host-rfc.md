# 导入插件宿主契约（返修稿）

- 文档状态：**阶段 A 与 B0 契约冻结复核通过**；B1–B6 产品代码工作包**未获批准、未启动**。
- 返修日期：2026-10-20
- 任务来源：`Agent_Task/Task4.md` §2.3 + `Agent_Task/Task4_Back_task.md`
- 依据：`项目规范.md` §2.3 / §3.2 / §5 / §8.3 / §9.3 / §12；`docs/skeleton/skeleton-domain-rfc.md`
- 与 `docs/skeleton/skeleton-domain-rfc.md` 是配套文档：Domain 定**数据**，本文定**边界与流程**

### 已裁定事项在本文件中的落点

| 裁定 | 落点 | 决策号 |
|---|---|---|
| 编译期静态注册 | §3 | D26 |
| 宿主统一随机 ULID | §7.3 | D25 |
| 不提供插件版本迁移钩子 | §8 | D28 |

---

## 1. 目标与纪律

### 1.1 目标

定义 Loongbones 与**导入插件**之间的边界：宿主提供什么、插件交回什么、
失败怎么报、进度/取消怎么做、版本不兼容怎么处理。

### 1.2 纪律（硬约束）

| 约束 | 来源 |
|---|---|
| 插件只产出 **canonical Domain 数据**；**不得**把 Spine/DragonBones 序列化作为隐藏主数据或内部中转格式 | `项目规范.md` §2.3.6 |
| 编辑器主代码**不得硬编码**某种外部格式语义；格式支持只经插件 | `项目规范.md` §3.2 |
| 插件运行在**宿主进程内**，**无崩溃隔离**；不得对外宣称有沙箱 | 本文 §3.1 |
| 插件**不得**自行读写任意路径；资源访问经宿主授权 | 本文 §4.2 |
| 插件**不得**直接弹 UI / 直接写工程；只交回调结构与诊断 | 本文 §5 |
| 宿主**不得**用插件私有类型作为持久化格式 | 本文 §7.4 |
| 插件**不得**自行生成持久化 ID；一律由宿主分配随机 ULID | 本文 §7.3；D25 |

---

## 2. 插件身份与声明

```jsonc
{
  "pluginId": "loongbones.importer.spine32",   // dot.case，全局唯一
  "pluginVersion": 1,                          // 单调递增整数（不做 semver 语义承诺）
  "hostApiVersion": { "min": 1, "max": 1 },    // 宿主 API 版本范围，闭区间
  "sourceFormats": [
    { "id": "spine.binary", "versions": ["3.2.01"] }
  ],
  "displayName": "Spine 3.2.01 骨架导入",
  "capabilities": ["skeleton.static"]          // 该插件支持的能力标签
}
```

- `hostApiVersion.max` 必须 ≤ 宿主实际版本，否则**注册失败**（不是运行时失败）。
- 插件**必须**声明 `sourceFormats`：宿主据此选择插件，**不做后缀名猜测**
  （`.skel`/`.bytes`/`.json` 都可能对应多种格式）。
- `pluginVersion` 用整数而非 semver：本契约**不承诺** semver 的兼容语义，
  版本不兼容一律显式拒绝（§8）。

---

## 3. 加载形态（已定案 D26：编译期静态注册）

### 3.1 定案内容

**首个 Spine 导入插件采用编译期静态注册**，宿主只依赖抽象接口：

- 插件实现一个 `ImportPlugin` 抽象接口，在编译期注册到宿主的插件表；
- 宿主**只依赖抽象接口**，不依赖任何具体格式的常量、魔数或类型
  （`项目规范.md` §3.2：主代码不得硬编码外部格式语义）；
- 插件实现不反向依赖宿主内部类型。

**本阶段明确不承诺**：

| 不承诺 | 说明 |
|---|---|
| 动态库 ABI | 不提供稳定的 C ABI、不保证跨编译器/跨版本二进制兼容 |
| 运行时发现 / 安装 | 不支持运行时扫描目录、动态加载/卸载、用户自装插件 |
| 进程隔离 / 沙箱 | 插件运行在宿主进程内，**无崩溃隔离**；插件段错误 = 宿主崩溃 |

> 这不是疏漏，而是**明言的边界**：把"运行时插件系统"的能力要求推后，
> 直到 `DECISIONS.md` V5（Qt 编译器）定案后再评估是否值得付出 ABI 冻结与进程隔离成本。

### 3.2 为什么不定成动态库 / 子进程（历史备选，仅作记录）

| 选项 | 未采用原因 |
|---|---|
| 进程内动态库 + C ABI | V5 未决（Qt 编译器影响核心库 ABI）；且常被误认为"有隔离"，实际没有 |
| 子进程 + IPC | 有隔离但代价显著（IPC、资源授权、性能）；当前无对应需求 |

### 3.3 迁移路径

将来若改加载器（V5 定案后）：`ImportPlugin` 抽象接口与值语义的候选/诊断结构**保持不变**，
只替换"如何找到并构造插件实例"。因此按静态注册实现**不会返工**。
改加载器须**另立决策**（D26 已注明）。

### 3.4 准入要求（因为没有崩溃隔离）

插件必须：不吞异常、不持有跨调用的对象生命周期、不跨模块分配/释放内存、
不自行读写任意路径（§4.2）、不弹 UI（§5）。

---

## 4. 宿主提供的能力（插件可见面）

### 4.1 最小接口面

| 能力 | 说明 |
|---|---|
| `IReadOnlySource` | 只读字节视图 + 逻辑名。插件**不得**据此推断真实文件系统路径 |
| `IResourceResolver` | 按**源内相对引用**（如 `.atlas.txt` 页头 `file` 给出的 PNG 名）解析伴随文件；解析不到 → 诊断，不猜测 |
| `IResourceStaging` | 宿主资源暂存服务：读入源 PNG 字节、暂存、写工程内相对路径、失败回滚。**插件不获得任意路径写权限** |
| `IAtlasBuilder` | 让插件把解析出的页/region 交给宿主，由宿主统一建 `Atlas/AtlasRegion/TextureAsset` |
| `IIssueSink` | 诊断汇聚（§6） |
| `IProgress` / `ICancelToken` | 进度汇报与取消（§7.2） |
| `IIdFactory` | 由**宿主**统一分配**随机 ULID**；插件**不得**自行造 ID、不得用源下标或源 hash 派生 ID（§7.3） |

> `IResourceResolver`（只读解析）与 `IResourceStaging`（写盘）是**两个接口**：
> 插件可以请求读，可以请求"把这个候选资源暂存到工程内 `<TextureAssetId>.png`"，
> 但**不能**自己选路径、不能直接写文件系统。资源落地策略见
> `docs/format/native-project-v2-skeleton.md` §7。

### 4.2 资源访问授权

- 插件**不得**自行打开任意路径（不得 `fopen(argv)` 式行为）。
- 伴随文件（图集/纹理）只能通过 `IResourceResolver`，且**只能按源内相对引用**解析。
- 宿主保留「拒绝解析」的权利（如越出授权根目录、命中符号链接逃逸）。
- 目的：把"插件能碰什么"收敛到一处，避免插件成为任意文件读取的跳板。

---

## 5. 插件交回什么

```jsonc
{
  "candidates": [ /* 0..N 个候选对象，全部为 canonical Domain 对象 */ ],
  "report": {
    "sourceRef": { "formatId": "spine.binary", "formatVersion": "3.2.01",
                   "sourceHeaderHash": "…",      // 源自带头 hash 原文，可选
                   "contentHash": "…",           // 宿主/插件对 .skel.bytes 全文算的 SHA-256
                   "sourceUnit": "b0001s3" },
    "summary": { "bones": 29, "slots": 23, "attachments": 24 },
    "diagnostics": [ /* ImportDiagnostic[] */ ],
    "unsupported": [ /* { feature, count, sampleLocation }[] */ ]
  }
}
```

- 候选对象是 **canonical Domain 对象**（`Skeleton` / `Atlas` / `TextureAsset` …），
  不是插件私有结构、不是源字节。
- 插件**不得**写工程、不得弹 UI、不得修改已存在的对象。
- `summary` 只统计**本任务范围内**的对象（`skeleton-domain-rfc.md` §10.1）；
  范围外内容**不进入** summary，只在 `unsupported` 里计数。
- `unsupported` 必须与 `skeleton-domain-rfc.md` §10.2 的"不支持且有诊断"清单**逐项对应**：
  源里出现但本任务不实现的能力，必须列出**计数与样例位置**，便于用户核对损失。

---

## 6. 诊断契约

```jsonc
{
  "code": "SKELETON_ATTACHMENT_REGION_UNRESOLVED",
  "severity": "error" | "warn" | "info",
  "message": "…",                    // 面向用户，不含内部堆栈
  "location": "/skeletons/0/skins/0/entries/3",  // canonical 侧 JSON Pointer（可空）
  "sourceLocation": { "section": "skins", "offset": 12345 }  // 源侧定位（字节偏移）
}
```

- **必须同时**给出 canonical 侧位置和源侧位置：只有一侧用户都无法定位。
- `severity = error` → 候选被丢弃（§7.1）。
- 插件**不得**把诊断写成自由文本后期望宿主解析；`code` 必须来自约定枚举。

---

## 7. 流程与失败语义

### 7.1 失败即丢弃（原子性）

- 插件产出的是**候选对象集合**；任一 `error` 级诊断 → 整个候选集被丢弃，**不部分提交**。
- 与 `项目规范.md` §9.3「打开失败必须一次性报清」同理念：宁可整体失败，
  也不提交一个"看起来能打开、实际缺件"的工程。
- 已创建的临时对象由宿主回收；插件不得持有跨调用的对象生命周期。

### 7.2 进度与取消

- 插件必须在**有界**的循环里周期性检查 `ICancelToken`；取消后尽快返回，
  返回值标记为 `cancelled`，候选一律丢弃。
- 进度只用于展示，**不得**影响结果（不允许"进度不同 → 结果不同"）。

### 7.3 ID 归属（已定案 D25）

- ID **由宿主统一分配**（`IIdFactory`），且一律为**随机 ULID**；
  插件**不得**自行造 ID、**不得**用源数组下标、**不得**用源内容 hash 派生 ID。
- 插件只提供**稳定键**（`skeleton-domain-rfc.md` §7.3），且稳定键仅在**单次导入内**
  把源引用解析到本次已分配的 ID。
- 这样 ID 生成只有宿主一个实现点，插件无法各自造 ID；
  重复导入默认**创建副本**（新 ULID），源 hash 只作来源元数据 / 重复提示。

### 7.4 持久化边界

- 工程内只保存 `sourceRef` **元数据**（格式 id / 版本 / `sourceHeaderHash` / `contentHash` / 源内逻辑单位 / 插件 id 与版本）。
  `sourceRef` 是**整个可选对象**：插件导入时必定产出；原生创建的骨架没有它。不得为"必须有个值"而伪造来源信息
  （与 `docs/format/native-project-v2-skeleton.md` §3.7 一致）。
- `sourceHeaderHash` = Spine 二进制头 `hash` 字段**原文**（源自带，无算法，可选）；
  `contentHash` = **宿主/插件对 `.skel.bytes` 全文**计算的 SHA-256。
  二者来源与算法不同，**不得**混为一字段（与 `skeleton-domain-rfc.md` §8.3 一致）。
- **不保存**伴随 `.atlas.txt` / `.png` 的摘要。
- **不保存**插件二进制、插件私有结构、源字节、源绝对路径。
- 工程对插件**没有运行时依赖**：骨架已是 canonical Domain 数据，插件缺失不影响打开（§8）。

### 7.5 资源落地（B0 冻结）

| 项 | 规则 |
|---|---|
| 导入产出的 `TextureAsset.storage` | 恒为 `"inProject"` |
| `location` | `assets/textures/<TextureAssetId>.png`（工程相对路径；ID 由宿主分配，故路径不可被用户重命名影响） |
| 写盘时机 | Domain 候选校验通过**之后**；先临时文件 → 校验 hash/尺寸 → 原子替换 |
| 失败回滚 | 回滚本次新增的资源文件；旧工程保持有效；`project.json` 不出现指向缺失文件的窗口 |
| 插件权限 | 只读源（`IResourceResolver`）+ 请求暂存（`IResourceStaging`）；**无任意路径写权限** |
| 外部引用 | `external` 保留给用户显式创建；导入器永不产出 |

完整规则（含 Format 层 `location` 形状校验与错误码）见
`docs/format/native-project-v2-skeleton.md` §7。

---

## 8. 版本与兼容性

| 情形 | 行为 |
|---|---|
| 插件 `hostApiVersion.max` > 宿主版本 | **注册失败**，并在插件管理界面显式列出原因 |
| 插件 `hostApiVersion.min` > 宿主版本 | 同上 |
| 源格式版本不在插件声明的 `versions` 内 | **明确诊断 + 拒绝导入**，**不猜测、不降级尝试** |
| 已导入工程的 `sourceRef.importedBy.pluginVersion` > 当前插件版本 | 明确诊断 + **拒绝自动重导**；不猜测、不降级 |
| 插件缺失（工程引用了未安装插件的 `pluginId`） | 工程仍可打开（骨架已是 canonical 数据，不依赖插件）；只是不能重导 |

### 8.1 插件版本升级：不提供迁移钩子（已定案 D28）

- **不提供插件版本迁移钩子**：插件升级**不自动改写**任何已有 canonical 数据。
- 插件版本号仅作**来源记录与诊断**（写入 `sourceRef.importedBy`）。
- 更新/重导若实现，必须是**用户显式触发**；且必须遵守稳定 ID 映射规则
  （`skeleton-domain-rfc.md` §7.4），不得对当前内容重新 hash 派生 ID。
- 理由：自动改写 canonical 数据等于引入第二条迁移路径，其失败面与审计成本
  远高于"让用户显式决定"。

---

## 9. 验收方式（阶段 B）

| 项 | 验收 |
|---|---|
| 宿主只依赖抽象接口 | 主代码里 grep 不到具体格式常量/魔数 |
| 无隐藏主数据 | 工程 JSON 中不含任何源字节；`sourceRef` 只有元数据 |
| 随机 ID | 重复导入同一源 → 两套 ID 不相同且均合法；ID 不由名称/下标/hash 推出 |
| 失败原子性 | fixture `03-bad-parent-index` → 候选全丢弃，工程不变 |
| 截断 | fixture `04-truncated` → EOF 诊断，不部分提交 |
| 非有限浮点 | fixture `05-non-finite-float` → `SKELETON_FLOAT_NON_FINITE` + 字节偏移，不部分提交 |
| 取消 | 大样本导入中途取消 → 无残留对象 |
| 资源授权 | 插件无法通过源内引用逃出授权根目录；插件无任意路径写权限（只经 `IResourceStaging`） |
| 资源落地 | 导入产出 `storage = "inProject"`、`location = assets/textures/<id>.png`；Domain 校验通过后才写盘；写失败回滚且旧工程有效 |
| 范围一致 | 只导入 §10.1（`skeleton-domain-rfc.md`）范围内能力；范围外能力在 `unsupported` 列出计数 |
| 诊断等级 | error 拒绝整个候选；warning 仅在调用方显式 `acceptWarnings` 后可提交 |

---

## 10. 与既有决策的关系

- `DECISIONS.md` **D26**（静态注册）：本文 §3；改加载器须另立决策。
- `DECISIONS.md` **V5**（Qt 编译器）：决定将来能否安全换成动态库/子进程；当前按 §3.1 规避。
- `DECISIONS.md` **D25 / ADR-0004**（随机 ULID）：§7.3 的直接依据。
- `native-project-v1-rfc.md` 的未知字段机制：**不用于**承载源格式数据（§1.2 硬约束）。
- **v2 schema 冻结**：`docs/format/native-project-v2-skeleton.md`（B0）；本文的 `sourceRef`、资源落地、诊断等级须与其一致。
