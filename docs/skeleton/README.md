# 骨架模块设计文档（Task 4 · 阶段 A / 阶段 B 前置）

> **状态：阶段 A 与 B0 复核通过；B1 实现与验证完成、待复核；B2/B3 实现完成；B4 Reader 全语料通过（Full 1113/1113、Primary 963/963，manifest 锁定样本 16/16），交付边界为「结构可读」；B5 Atlas Reader + Mapper 实现完成（32 项测试、全语料 region 未命中 0）；B6 实现完成（25 项测试、语料 1113 单位 prepare 全通过、8 个单位提交+重开闭环一致、7 个 CTest 目标绿）。**
> 阶段 A 只做**设计契约与证据**：不写产品读取器、不写 GUI、不写渲染。
> 返修依据：`../Agent_Task/Task4_Back_task.md`；三轮返修记录见 `ACCEPTANCE.md` §7 / §8 / §9。
> 阶段 B 启动门槛与工作包拆分见 `../Agent_Task/Task4_Implementation_Plan.md`；
> B0 验收报告（含退回补项记录）见 `B0_REPORT.md`。
> **证据范围**：本目录与 v2 schema 的语料证据全部来自 Spine 3.2.01；尚未完成 DragonBones 对照，
> 相关结论不得表述为跨格式通用结论。

## 文档

| 文件 | 内容 |
|---|---|
| [`skeleton-domain-rfc.md`](skeleton-domain-rfc.md) | **Skeleton Domain 与原生格式兼容提案**：职责边界、v2 兼容与迁移契约（D24）、数据模型提案、Task 4 垂直切片范围、语义映射、ID/稳定键（D25）、诊断（含非有限浮点）、支持矩阵 |
| [`plugin-host-rfc.md`](plugin-host-rfc.md) | **导入插件宿主契约**：插件身份与声明、编译期静态注册（D26）、宿主可见面、插件交回什么、诊断、失败原子性、版本兼容（D28） |
| [`field-mapping.md`](field-mapping.md) | **Spine 3.2.01 → canonical 字段映射表**（逐源字段：证据 → 转换规则 → 语料实测），并区分"解析/证据映射 / 未来模型提案 / Task 4 实现范围"三层 |
| [`sample-manifest.md`](sample-manifest.md) | **样本 manifest**：16 个样本的三件套**相对路径 + 字节数 + SHA-256**（skeleton / atlas / PNG，PNG 按页列出）、版本、计数、特性标签、解析结论（只含元数据） |
| [`fixtures/README.md`](fixtures/README.md) | **合成 fixture**（5 个，不含游戏资产）与预期结果 |
| [`ACCEPTANCE.md`](ACCEPTANCE.md) | 阶段 A 验收报告 + 三轮返修记录（§7 / §8 / §9） |
| [`B0_REPORT.md`](B0_REPORT.md) | **阶段 B B0 验收报告**：契约冻结清单、逐项对照、待确认项 |
| [`B1_REPORT.md`](B1_REPORT.md) | **阶段 B B1 验收报告**：Atlas + PNG 独立导入、资源提交闭环、语料 smoke 与测试结果 |
| [`B3_REPORT.md`](B3_REPORT.md) | **阶段 B B3 验收报告**：v1/v2 Codec、v1→v2 迁移、未知字段锚点与测试结果 |
| [`task4_B4.md`](task4_B4.md) | **阶段 B B4 Reader 报告**：二进制布局证据、测试/语料结果、Full/Primary 口径与退出码、交付边界与 B5 交接清单 |
| [`B5_REPORT.md`](B5_REPORT.md) | **阶段 B B5 Atlas Reader + Mapper 报告**：图集文本解析、Source→canonical 映射规则、测试与语料对账 |
| [`B6_REPORT.md`](B6_REPORT.md) | **阶段 B B6 静态插件宿主报告**：插件契约与静态注册、事务提交语义、端到端闭环与语料实测 |
| [`../Agent_Task/DataType_Report.md`](../Agent_Task/DataType_Report.md) | **数据类型报告**：已实现 canonical 类型总表、重建文件核对结果、后续待实现类型与残余风险 |
| [`../format/native-project-v2-skeleton.md`](../format/native-project-v2-skeleton.md) | **v2 schema 冻结稿**（B0 产出）：完整 JSON 示例、字段必需性/默认值、枚举、ID、未知字段锚点、能力、迁移、错误码、资源落地、诊断等级 |

## 工具

| 路径 | 说明 |
|---|---|
| `../../tools/skeleton-probe/` | 只读探针：`probe.mjs`（全量统计 + 伴随文件 hash）、`png-page-probe.mjs`（图集页坐标空间判定）、`make-fixture.mjs`（合成 fixture 生成）、`emit-samples.mjs`（样本 manifest 数据表） |

## 三条不可动摇的边界

1. **Spine Bone ≠ Scene Node**（D27）。骨架是与 `Atlas` 同级的 Project 级资产；
   Task 4 导入**不创建 Scene 实例**；未来放置须由专用骨架实例类型引用 Skeleton。
2. **不得把源序列化塞进工程**。既不能用未知字段承载 Spine 字节
   （`项目规范.md` §2.3.6），也不能把它当内部中转格式。工程内只存 `sourceRef` 元数据。
3. **不得用源数组下标或源内容 hash 做持久化 ID**（D25）。一律宿主随机 ULID；
   稳定键只在单次导入内做引用映射；重复导入默认创建副本。

## 已裁定事项（不再是待决项）

| 裁定 | 决策号 |
|---|---|
| 原生格式采用 v2 承载 Skeleton；v1 Reader 拒绝 v2；纯函数迁移 + 未知字段保留 + 保存才写 v2 | D24 |
| 含 `skeletons[]` 的 v2 文档必须声明 `loongbones.skeleton.v1` 为 **required**；该能力不替代版本检查 | D24（2026-10-20 追加裁定） |
| 旧 v1 Reader 读到 `formatVersion = 2` 固定返回 `FORMAT_VERSION_UNSUPPORTED` | D24（2026-10-20 追加裁定） |
| 源带头 `hash` 与文件摘要拆为两个字段：`sourceRef.sourceHeaderHash`（源自带、可选）与 `sourceRef.contentHash`（宿主对 `.skel.bytes` 全文算的 SHA-256） | D25（2026-10-20 追加裁定） |
| 持久化对象一律随机 ULID；源 hash 仅作来源/重复提示 | D25 |
| 首个 Spine 插件编译期静态注册；不承诺 ABI / 运行时安装 / 崩溃隔离 | D26 |
| Skeleton 只作 Project 级资产，不入 Scene | D27 |
| 不提供插件版本迁移钩子；升级不自动改写 canonical 数据 | D28 |

## 仍待验证（非待决，是有验证计划的未知项）

T-A1（rotation≠0 与页缩放复合）、T-A2（继承布尔分布）、T-B1（`pma:true`）、
T-B2（`split`/`pad`）、T-B3（尾随空格降级匹配）、T-B4（多皮肤/多页图集）、
T-B5（`deform offset` 形态分布）—— 清单与责任阶段见 `skeleton-domain-rfc.md` §11.1。

## 相关任务文件

- [`../Agent_Task/Task4.md`](../Agent_Task/Task4.md) — 任务正文
- [`../Agent_Task/Task4_Back_task.md`](../Agent_Task/Task4_Back_task.md) — 阶段 A 返修规格（本次返修依据）
