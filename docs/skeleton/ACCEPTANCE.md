# Task 4 阶段 A 验收报告（返修稿）

- 日期：初稿 2026-10-20；**返修 2026-10-20**（依据 `Agent_Task/Task4_Back_task.md` §2/§3）；**追加返修 2026-10-20**（§4 第二轮追加项、§5 第三轮追加项）
- 范围：`Agent_Task/Task4.md` §2.1 – §2.4（阶段 A：只读核实、契约、样本、fixture、报告）
- 结论：初稿"阶段 A 交付完成"；**复核未通过，已返修，现申请复核**。
  阶段 B **未获批准**、未启动（无产品读取器实现）。
- 说明：§1–§6 保留为**初稿的真实运行记录**（数字与日期未改写）；
  本次复核发现与返修结果见 **§7**；返修后的当前口径以各 RFC 为准。

---

## 1. 交付清单（初稿）

| # | 任务书要求 | 交付物 | 状态 |
|---|---|---|---|
| 1 | §2.2.1 Domain 职责边界 | `skeleton-domain-rfc.md` §3 | ✅ 明确「Bone ≠ SceneNode」；初稿将放置方式列为待决 Q-A1（返修后已定案，见 §7） |
| 2 | §2.2.2 与 v1 的兼容方式 | `skeleton-domain-rfc.md` §4 | ✅ 三选项对比 + 推荐 B + 迁移/降级风险；初稿留裁定请求 R-A1（返修后已定案，见 §7） |
| 3 | §2.2.3 语义细节（证据/规则/golden 计划） | `skeleton-domain-rfc.md` §6、`field-mapping.md` | ✅ 每项含证据来源、转换规则、golden 值计划 |
| 4 | §2.2.4 ID 与稳定键 | `skeleton-domain-rfc.md` §7 | ✅ 已实测名称唯一性，给出稳定键与消歧规则；初稿派生策略待裁定（返修后按 D25 定案） |
| 5 | §2.2.5 未知/不支持/诊断 | `skeleton-domain-rfc.md` §8/§9、`plugin-host-rfc.md` §6 | ✅ 三分法 + 诊断码 + 失败原子性 |
| 6 | §2.2.6 语义映射 / 保留片段 / 不支持三类边界 | `skeleton-domain-rfc.md` §8.1/§8.2 | ✅ 明确**否决**「保留源片段」进工程（规范 §2.3.6） |
| 7 | §2.2.7 支持矩阵 | `skeleton-domain-rfc.md` §10、`field-mapping.md` §11 | ✅ 分类矩阵（返修后改为按三层范围拆分，见 §7） |
| 8 | §2.3 插件宿主契约 | `plugin-host-rfc.md` | ✅ 身份/加载/输入输出/诊断/失败/版本（加载形态返修后定案 D26） |
| 9 | §2.1.2 样本 manifest | `sample-manifest.md` | ✅ 16 样本：SHA-256、相对路径、版本、标签、结论（返修后补齐三件套锁定，见 §7） |
| 10 | §2.4 合成 fixture ≥3 | `fixtures/`（**5 个**）+ README | ✅ 不含游戏资产（返修后新增 05-non-finite-float） |
| 11 | §2.4 独立探针 | `tools/skeleton-probe/`（**4 个 `.mjs`** + README） | ✅ 只读、可复跑（脚本清单见 §7.4） |
| 12 | §2.1.4 更新 `DECISIONS.md` 证据修订 | `DECISIONS.md` | ✅ U4/U6/U7/V6/V7/V8/V2 修订 |
| 13 | §5 报告 | 本文件 | ✅ |

---

## 2. 只读核实结果（关键数字）

语料：**1113 个 `.skel.bytes`**（Full）/ 963（primary）。全部 `3.2.01`。

| 项 | 结果 |
|---|---|
| 解析成功（strictEof） | **1113 / 1113**，失败 0 |
| 引用闭合 | **1113 / 1113**（骨骼父索引越界 0、前向引用 0、自环 0；插槽骨骼越界 0；IK/TC 目标越界 0） |
| 名称唯一性 | 骨骼重名 **0**、插槽重名 **0**、同 slot 内附件重名 **0** |
| `nonessential` | true **4** / false **1109**；`imagesPath` 非空 **4** |
| 三件套齐全 | **1110 / 1113**（`.atlas.txt` 1111、`.png` 1110） |
| 附件 `path` | 显式 path **84**，且 **84/84** 与附件名不同 |
| 附件→region 查找（`path ?? name`） | 命中 **47,705** / 未命中 **4**（根因：附件名尾随空格） |
| 图集 | 页 **1111**、region **43,704**；`rotate: true` **17,734**（40.6%）；**trim 0**；`index` 非 -1 **0**；`pma` **全部未声明**；`format` 全 `RGBA8888` |
| **页声明 size vs PNG IHDR** | 一致仅 **4 / 1111**；PNG 恒为 2 的幂 |
| 坐标空间判定 | 「region 坐标 = 页声明空间，PNG 为更高分辨率同一图」**26 支持 / 0 反对 / 3 不确定 / 1 不支持的 PNG 形态** |
| mesh UV | 越出 `[0,1]` **0** → regionUVs 归一化判定成立 |
| 三角索引越界 | **0** |
| 多皮肤单位 / 多页图集 | **0 / 0**（语料无法覆盖，需合成 fixture） |
| 动画 | 动画 **16,046**、时间线 **4,954,836**（Full）；kind 全集为 rotate/translate/scale/shear/attachment/color/deform/ik/drawOrder/transform，**event 时间线 0** |

---

## 3. 实证解决的不确定项

| 编号 | 原状态 | 现状态 | 依据 |
|---|---|---|---|
| **U4** 附件 `path` 与名称不一致比例 | 待验证 | **已量化：84 例，且 84/84 不一致** | 探针 §4 |
| **U6** 图集 region 名与附件名关系 | 待验证 | **已量化：`path ?? name` 命中率 99.99%；4 例未命中因尾随空格** | 探针 §6 |
| **U7** `nonessential` 覆盖率 | 待验证 | **已量化：true 4 / false 1109** | 探针 §1 |
| **V6** 名称唯一性 | 待验证 | **已量化：骨骼/插槽/附件名均零重名** → 名称可作稳定键（附消歧规则） | 探针 §3 |
| **V7** `nonessential` 语义 | 待验证 | 同 U7；`imagesPath` 非空 4 例 | 探针 §1 |
| **V8** linked mesh 出现率 | 待验证 | **已量化：2 例（Full）**；且 deform 时间线 335,817 | 探针 §7 |
| **V2** 图集缩放承载层 | 阻塞 | **承载层已定（页层）**；rotation≠0 的复合仍待验证（T-A1） | `atlas-page-space-verdict.json` |

---

## 4. 初稿遗留的待验证项

T-A1（rotation≠0 与页缩放复合）、T-A2（继承布尔分布）、T-B1（`pma:true`）、
T-B2（`split`/`pad`）、T-B3（尾随空格降级匹配）、T-B4（多皮肤/多页图集）。
（返修后新增 T-B5：`deform offset` 形态分布；清单见 `skeleton-domain-rfc.md` §11.1。）

---

## 5. 纪律自查

| 纪律 | 自查结果 |
|---|---|
| 未移动/改写/删除/复制游戏文件 | ✅ 探针全程只读；fixture 为脚本合成 |
| 未把游戏资产写入仓库 | ✅ 仓库内只有元数据（相对路径 + hash + 计数）与合成 fixture |
| 未写绝对路径/盘符/用户名 | ✅ 语料根由参数或环境变量提供，不入产物 |
| 未把源序列化塞进工程/未知字段 | ✅ 明确否决（规范 §2.3.6） |
| 未改已审阅的 v1 RFC | ✅ 只新增 `docs/skeleton/`，未改 `docs/format/native-project-v1-rfc.md` |
| 未改 Task 1/2 已发布 ID 与语义 | ✅ 提案只在 v1 之上"新增"，未重命名/删除既有字段 |
| 未联网、未引入依赖 | ✅ 探针零第三方依赖（仅 `node:zlib` + 标准库） |
| 未进入阶段 B | ✅ 无产品读取器实现 |

---

## 6. 如何复跑

```bash
# 全量只读扫描（约 1113 个文件）
node tools/skeleton-probe/probe.mjs "$LOONGBONES_CORPUS_ROOT" -o tools/skeleton-probe/out

# 图集页坐标空间判定
node tools/skeleton-probe/png-page-probe.mjs "$LOONGBONES_CORPUS_ROOT" --limit 24

# 合成 fixture 生成 + 回读自校验
node tools/skeleton-probe/make-fixture.mjs --verify

# 样本 manifest 数据表（从上面的 out/ JSON 摘录）
node tools/skeleton-probe/emit-samples.mjs
```

详见 `../tools/skeleton-probe/README.md`。

---

## 7. 本次复核发现与返修结果（2026-10-20）

### 7.1 复核发现（初稿的问题）

| # | 发现 | 初稿的位置 |
|---|---|---|
| F1 | Project 顶层出现与 Skeleton 并列的 `skins[]`，与"Skin 归属 Skeleton"自相矛盾；且 v1 顶层本无该集合 | `skeleton-domain-rfc.md` §3.1 |
| F2 | §4 论证自相矛盾：一边说"选项 A 仅因 required capability 才安全"，一边又把 A 当可行选项推荐 B | §4.1/§4.2/§4.3 |
| F3 | 迁移契约缺版本字段、`minimumReaderVersion`、能力字段与保存时机；且未说明与 Task 3 codec 现有能力的差距（易被读成"已有 v2 Reader/Writer"） | §4.4 |
| F4 | ID 方案留了两个互斥选项（hash 派生 / 随机 ULID）与一条裁定请求，未按已定案的 D25 收敛 | §7.3 |
| F5 | 模型提案（§5）与 Task 4 实际实现范围未区分，mesh/IK/动画被写成"首批支持" | §5.4/§5.5/§6.11/§6.12/§10 |
| F6 | 无"源二进制非有限浮点（NaN/Inf）"规则，也无稳定诊断码 | §9 缺失 |
| F7 | Scene 放置、插件加载形态、插件版本迁移钩子仍列为待裁定 | §3.2/§11.1、`plugin-host-rfc.md` §3/§8 |
| F8 | 插件契约 §7.3 仍写"派生策略唯一实现点"，与随机 ULID 不符 | `plugin-host-rfc.md` §7.3 |
| F9 | 样本 manifest 只锁 `.skel.bytes`，未锁 `.atlas.txt` / `.png` | `sample-manifest.md` |
| F10 | ACCEPTANCE.md 称探针"3 脚本"，与目录实际 **4 个 `.mjs`** 不符 | 本文件 §1 |
| F11 | 文档日期写成 `2026-03-03`（与实际运行日期不符） | 多份文档 |
| F12 | 状态文字仍是"待负责人裁定"，与 D24–D28 已定案不一致 | `README.md`、`Agent_Task/Task4.md`、`STRUCTURE.md` |

### 7.2 返修结果

| # | 返修 | 落点 |
|---|---|---|
| F1 | 顶层只保留 `skeletons[]`；明确 **Skin 唯一归属 `Skeleton.skins[]`** | `skeleton-domain-rfc.md` §3.1 |
| F2 | §4 改为"已裁定 v2"；A/C 的历史备选分别说明真实风险（未知字段不透传语义安全 / required capability 只是拒绝语义），不再自相矛盾 | §4.1–§4.3 |
| F3 | 迁移契约补齐版本字段表、`minimumReaderVersion=2`、能力字段、"先入内存、保存才落盘"；并新增 §4.5 列出与 Task 3 codec 的 **7 项能力缺口**作为阶段 B 前置任务，明确不宣称已有 v2 Reader/Writer | §4.4–§4.5 |
| F4 | §7 按 D25 重写：一律随机 ULID；源 hash 只作来源元数据/重复提示；稳定键仅单次导入内引用；重复导入默认创建副本；去重提示不得隐式复用 ID | §7.1–§7.4 |
| F5 | 新增 §1.1"模型提案 vs 垂直切片"、§5.6 实现范围；§10 拆成 10.1（Task 4 范围）/10.2（不支持且有诊断）/10.3 拒绝/10.4 待验证；`field-mapping.md` 增加"三层范围"表并重写 §11 | §1.1/§5.6/§10、`field-mapping.md` 开头 + §11 |
| F6 | 新增 §6.11 非有限浮点规则 + §9.1 诊断码 `SKELETON_FLOAT_NON_FINITE`；新增 fixture `05-non-finite-float`（NaN 位于字节偏移 83） | §6.11/§9.1、`fixtures/` |
| F7 | Scene 放置写成决定（D27）；插件静态注册写成决定（D26）；插件版本迁移钩子明确"不提供"（D28） | §3.2/§9.4、`plugin-host-rfc.md` §3/§8.1 |
| F8 | §7.3 改为"宿主统一随机 ULID；插件不得自行造 ID/不得用下标或 hash 派生" | `plugin-host-rfc.md` §7.3 |
| F9 | 探针新增伴随文件 sha256 + 字节数记录；manifest 表 1 改为**三件套共同锁定**（skeleton / atlas / PNG） | `probe.mjs`、`sample-manifest.md` §1 |
| F10 | 本文件探针清单改为 **4 个 `.mjs`**（probe / png-page-probe / make-fixture / emit-samples） | 本文件 §1、§6 |
| F11 | 日期统一为 2026-10-20 | 全部 `docs/skeleton/` + `STRUCTURE.md` + `DECISIONS.md` |
| F12 | 三处状态统一为"阶段 A 待返修 / 返修完成待复核，阶段 B 未获批准"；已裁定项不再出现在待决列表；新增 Back Task 索引 | `README.md`、`Agent_Task/Task4.md`、`STRUCTURE.md` |

### 7.3 与已裁定决策的对应

| 决策 | 内容 | 落点 |
|---|---|---|
| D24 | 原生格式 v2 承载 Skeleton；v1 Reader 拒绝 v2；纯函数迁移 + 未知字段保留 + 先入内存、保存才写 v2 | `skeleton-domain-rfc.md` §4 |
| D25 | 持久化对象一律随机 ULID；源 hash 不入 ID 派生；重复导入默认创建副本 | §7 |
| D26 | 首个 Spine 插件编译期静态注册 | `plugin-host-rfc.md` §3 |
| D27 | Skeleton 只作 Project 级资产，不入 Scene；未来独立实例类型引用 | §3.2 |
| D28 | 不提供插件版本迁移钩子；升级不自动改写 canonical 数据 | §9.4 / `plugin-host-rfc.md` §8.1 |

`DECISIONS.md` 已含 D24–D28；`ADR-0004` 的 ID 策略已定案，Q1/Q2/Q3 已关闭，Q4（发布前合规审查）保留。

### 7.4 探针脚本清单（与目录一致，4 个 `.mjs`）

| 脚本 | 用途 |
|---|---|
| `probe.mjs` | 全量只读扫描（含伴随文件 sha256 + 字节数） |
| `png-page-probe.mjs` | 图集页坐标空间像素判据 |
| `make-fixture.mjs` | 合成 fixture 生成 + 回读自校验 |
| `emit-samples.mjs` | 从 `probe.mjs` 的 JSON 摘录样本 manifest 数据表 |

### 7.5 fixture 清单（5 个）

`01-minimal`、`02-nested-slots-regions`、`03-bad-parent-index`、`04-truncated`、
`05-non-finite-float`。预期结果见 `fixtures/README.md`。

### 7.6 返修后的验收自查

| 验收标准 | 结果 |
|---|---|
| 规范当前状态不再把已裁定事项标作"待裁定/待确认" | ✅ 现行规范文档（`docs/skeleton/*.md`、`Agent_Task/Task4.md`、`STRUCTURE.md`）中已无此类表述；例外仅为本报告 §7.1/§7.2 与 `Task4_Back_task.md` 里的**历史复核记录**（明确标注"初稿"） |
| Skin 所有权只有一种表达；Project 顶层无无归属 `skins[]` | ✅ §3.1 |
| Task 4 阶段 B 范围与支持矩阵一致 | ✅ §5.6 / §10.1 / `field-mapping.md` §11.1 |
| v2 迁移/旧 Reader 拒绝、随机 ULID + 副本导入、静态插件加载、无自动插件迁移、Skeleton 不入 Scene 可独立理解 | ✅ 各节均为"决定"句式，不依赖其它文档才能读懂 |
| 验收报告明确列出语料盲区与未经真实语料验证的行为 | ✅ §2、§4；`skeleton-domain-rfc.md` §11.2 |
| 不以合成 fixture 宣称实测兼容 | ✅ §5 纪律 + `fixtures/README.md` §2 明确 03/05 的"旧 Reader 能读"不是期望行为 |
| 只改文档与索引，不启动阶段 B | ✅ 未新增/修改任何产品代码 |

> **关于搜索口径**（Back Task §4.6）：验收时的"全仓无待决文字"指的是**规范当前状态**。
> 本报告 §7.1/§7.2 与 `Agent_Task/Task4_Back_task.md` 中出现的"待负责人裁定"等字样
> 属**历史复核记录**，用于说明初稿当时的问题，不构成现行状态；不应据此判定未返修。

### 7.7 返修中修正的一处既有文档不一致

`docs/adr/0004-stable-identity-strategy.md` §6 原写"同一素材导入两次 → 两次的导入对象
ID 集合相同（派生路径）"，与已定案的随机 ULID 矛盾，已改为"两次导入得到**不同** ID，
默认创建副本；沿用 ID 必须走持久化 source-key 映射"。

---

## 8. 第二轮复核发现与追加返修（2026-10-20）

依据 `Agent_Task/Task4_Back_task.md` §4（复核追加项）。§1–§7 保持为历史记录，不改写。

### 8.1 追加发现

| # | 发现 | 位置 |
|---|---|---|
| A1 | 骨架能力 `required` / `optional` 留待阶段 B 决定 | `skeleton-domain-rfc.md` §4.4/§4.5 |
| A2 | 旧 v1 Reader 遇 v2 的错误码写成"`FORMAT_VERSION_UNSUPPORTED`（或 `FORMAT_TOO_NEW`，视实现取哪一种）" | §4.1 |
| A3 | manifest 只有 hash 与字节数，**没有 `.atlas.txt` / `.png` 的相对路径**，无法唯一定位文件 | `sample-manifest.md` §1 |
| A4 | `field-mapping.md` 头部 `hash` 映射为 `sourceRef.sourceHash`，与 RFC/插件契约的 `contentHash` 不一致，且未注明算法与覆盖范围 | `field-mapping.md` §1 |
| A5 | §10.2 标题"仅解析保留 / 不支持且有诊断"与正文"工程内不留任何等价物"冲突 | `skeleton-domain-rfc.md` §10.2 |
| A6 | §7.6 的"全仓搜索无待决项"可能把 Back Task 历史文字里的"待负责人裁定"误判为现行状态 | 本文件 §7.6 |

### 8.2 追加返修结果

| # | 返修 | 落点 |
|---|---|---|
| A1 | 裁定：含 `skeletons[]` 的 v2 文档**必须**把 `loongbones.skeleton.v1` 声明为 **required**；不认识该能力的 Reader 必须拒绝；该能力**不替代** v2 版本检查 | `skeleton-domain-rfc.md` §4.4（+ §4.1、§4.5 第 5 项同步） |
| A2 | 移除二选一：旧 v1 Reader 读到 `formatVersion=2` **固定返回 `FORMAT_VERSION_UNSUPPORTED`**；`minimumReaderVersion` 超限继续用对应的 reader-version 错误码（现有 codec 为 `FORMAT_TOO_NEW`）。两条规则各自单一、互不替代 | §4.1、§4.4 |
| A3 | manifest §1 增加**三件套各自的相对语料根路径**；新增 §1b **逐页明细**（页序 / 页头 `file` / PNG 相对路径 / 字节 / SHA-256 / 页声明尺寸 / region 数）；新增 §1c 算法与覆盖范围。本批 16 样本图集**全部单页**，故每样本 1 个 PNG；若将来多页，§1b 不会遗漏 | `sample-manifest.md` §1/§1b/§1c；`probe.mjs` 新增 `pageDetails` 与伴随文件相对路径记录；`emit-samples.mjs` 输出三张表 |
| A4 | `hash` → `sourceRef.contentHash`；注明算法 **SHA-256** 与覆盖范围（**源骨架 `.skel.bytes` 全文**）；明确伴随文件摘要**不持久化**、不得与 `contentHash` 混称，将来若持久化须另起字段名 | `field-mapping.md` §1；`skeleton-domain-rfc.md` §7.2/§8.3 |
| A5 | §10.2 标题统一为"**不支持且有诊断（不保存源片段）**"，正文与标题一致；并注明：若要保留"解析了多少/跳过多少"的统计，必须单独定义并实现一个**不含源序列化**的解析统计字段 | `skeleton-domain-rfc.md` §10.2 |
| A6 | §7.6 改为"规范当前状态不再…"，并加搜索口径说明：本报告与 Back Task 中的历史字样属复核记录，不构成现行状态 | 本文件 §7.6 |

### 8.3 追加验收自查

| 追加验收标准 | 结果 |
|---|---|
| v2 版本拒绝、required capability 拒绝各自有单一明确规则与稳定错误码，且不互相替代 | ✅ §4.1/§4.4 各一条固定规则 |
| 16 个 manifest 样本的 atlas/PNG 路径与 hash 对应唯一；多纹理情况无遗漏 | ✅ §1 列三条路径；§1b 逐页；本批全部单页（语料整体 0 个多页图集） |
| `contentHash` 命名及 hash 覆盖范围一致且可复现 | ✅ `field-mapping.md` §1、`skeleton-domain-rfc.md` §7.2/§8.3 三处一致；复跑见 `probe.mjs` |
| §10.2 分类不暗示保存源字节或保留不支持语义 | ✅ 标题与正文均为"不保存源片段" |
| RFC、manifest、ACCEPTANCE、Task、README、STRUCTURE 当前状态一致；阶段 B 仍未获批准 | ✅ 状态统一为"阶段 A 待返修（返修完成，待复核）；阶段 B 未获批准" |

### 8.4 探针变更（可复跑）

- `probe.mjs`：新增 `atlasTxtRelPath` / `pngRelPaths` / `atlas.pageDetails[]`
  （页序、页头 `file`、PNG 相对路径、字节、SHA-256、声明尺寸、region 数）。
  PNG 路径由图集**页头 `file`** 解析，不由 skeleton 文件名猜测。
- 全量重跑通过；`emit-samples.mjs` 重新输出三张表（路径+锁定 / 逐页明细 / 结构结论）。

---

## 9. 第三轮复核发现与追加返修（2026-10-20）

依据 `Agent_Task/Task4_Back_task.md` §5。§1–§8 保持为历史记录，不改写。

### 9.1 发现

| # | 发现 | 位置 |
|---|---|---|
| B1 | **源头 `hash` 与计算摘要被混为一个字段**：`field-mapping.md` §1 把 Spine 二进制头里的 `hash` 写成"原样"映射到 `sourceRef.contentHash`，而同一行又把 `contentHash` 定义成导入器对 `.skel.bytes` 全文计算的 SHA-256。二者**来源与算法都不同** | `field-mapping.md` §1（并波及 `skeleton-domain-rfc.md` §8.3、`plugin-host-rfc.md` §5/§7.4） |
| B2 | **状态文字不一致**：第二轮返修提交后，`STRUCTURE.md` 仍写"复核追加返修中"，而 Task / README / Back Task 写"两轮返修完成，待复核" | `STRUCTURE.md` |

### 9.2 返修结果

| # | 返修 | 落点 |
|---|---|---|
| B1 | 拆成两个字段：**`sourceRef.sourceHeaderHash`**（可选）= Spine 二进制头 `hash` 字段**原文**，源自带、无算法、不截断不重算；**`sourceRef.contentHash`** = **宿主/插件**对 `.skel.bytes` **全文**计算的 **SHA-256**。`field-mapping.md` §1 的头部表不再写"原样"映射，并新增对照表（来源 / 算法 / 覆盖范围 / 必填）。manifest 的 SHA-256 定位为**外部复现锁定值**，与 `contentHash` 同算法同范围但由探针离线计算 | `field-mapping.md` §1（头部表 + 对照表）；`skeleton-domain-rfc.md` §8.3（字段表 + 拆分说明）、§10.1（实现范围措辞）；`plugin-host-rfc.md` §5（返回结构）、§7.4（持久化边界）；`sample-manifest.md` §1c（两种 hash 关系表） |
| B2 | 所有现行状态统一为「**阶段 A 返修完成，待复核；阶段 B 未获批准、未启动**」 | `Agent_Task/Task4.md`、`Agent_Task/Task4_Back_task.md`、`STRUCTURE.md`、`docs/skeleton/README.md`。历史记录（本报告 §7–§9、Back Task 历史条目）中的阶段状态按要求**不改写** |

### 9.3 追加验收自查

| 追加验收标准 | 结果 |
|---|---|
| Spine 头部源 `hash` 与文件 SHA-256 `contentHash` 有不同名称、来源、算法和去向；无字段误映射 | ✅ 名称 `sourceHeaderHash` / `contentHash`；来源「源自带头字段」/「宿主读文件后计算」；算法「无」/「SHA-256」；去向「可选来源元数据」/「重复提示」 |
| 插件结果、RFC、字段表、manifest 对两种 hash 的定义一致 | ✅ `plugin-host-rfc.md` §5/§7.4、`skeleton-domain-rfc.md` §8.3、`field-mapping.md` §1、`sample-manifest.md` §1c 四处一致 |
| `Task4.md`、Back Task、`STRUCTURE.md`、`docs/skeleton/README.md` 当前状态一致；阶段 B 仍未获批准 | ✅ 四处均为「阶段 A 返修完成，待复核；阶段 B 未获批准、未启动」 |

### 9.4 本轮未改动的部分

- 未新增/修改任何产品代码；未启动阶段 B。
- 未重跑探针（本轮仅字段命名与状态文字，无统计数据变化）；manifest 的数值表保持上轮实测结果。

## 10. 阶段 A 复核结论（2026-10-20）

- 第三轮字段拆分复核通过：Spine 头部 `hash` → 可选 `sourceRef.sourceHeaderHash`（源原文，无哈希算法）；源文件全文 SHA-256 → 必需 `sourceRef.contentHash`。字段映射、Domain RFC、插件输入输出契约和 manifest 的定义一致。
- 三轮文档返修验收通过。阶段 A **批准完成**。
- 阶段 B **未获批准、未启动**；须负责人另行明确批准后才可开始。
- 本轮未新增/修改产品代码；未重跑探针。返修方报告 fixture 自检通过、lint 0 诊断，本复核未重复执行这些工具。
