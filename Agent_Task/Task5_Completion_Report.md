# Task 5 归档报告：第二批静态数据的建模与导入闭环

- 状态：**已完成并归档（2026-10-24）** —— 阶段 A（A1–A6）+ 阶段 B（B1–B5）全部完成并通过复核
- 归档后：Task 5 **不再新增改动**；遗留项已逐项交接（§4），后续问题立新 Task
- 配套文档：`Agent_Task/Task5.md`（规格与进度）、`Task5_Implementation_Plan.md`（方案）、
  `Task5_B1B2_Review.md` / `Task5_B3_Review.md` / `Task5_B4_Review.md` / `Task5_B5_Review.md`（各包复核）

---

## 1. 交付总览

| 工作包 | 交付 | 复核 |
|---|---|---|
| **阶段 A**（A1–A6） | 语料取证 + 模型 RFC + v3 格式提案 + 16 个合成 fixture + golden | **验收通过**（`Task5_A6_Acceptance.md`） |
| **B1** Domain 第二批类型与校验 | `MeshAttachment` / `WeightedMesh` / `LinkedMesh` / `IkConstraint` / `TransformConstraint` + 9 个新诊断码 | **二次复核通过** |
| **B2** 原生格式 v3 | `formatVersion = 3`、`migrateV2ToV3()` 纯函数、写出统一 v3、v1/v2 Reader 对 v3 拒绝 | **二次复核通过** |
| **B3** Reader / Mapper 扩展 | 6 类附件落地 + 两阶段父引用解析（D38）+ 16 fixture 逐条断言 golden | **二次复核通过** |
| **B4** 插件诊断与报告 | 域码 → 稳定 ImportCode 映射、账本第二批条目消失 | **三次复核通过** |
| **B5** 语料闭环与回归 | 双口径 + 逐项语义比对 + 失败原子性 + 工具登记 | **复核通过（2026-10-24）** |

---

## 2. 最终验收证据（本对话独立复跑，2026-10-24）

| 项 | 结果 |
|---|---|
| 构建（干净目录 `build_b5`） | `cmake` 0 / `mingw32-make` **0**，**error 0、warning 0** |
| `ctest` | **7 / 7 通过，exit 0** |
| 断言总数 | core **135**、format **86**、binary **47**、mapper **48**、skeleton_import **40**、atlas **9** —— 全通过、零回归 |
| **双口径**（D12 不混分母） | Full **1113 / ok 1113 / fail 0**；Primary **963 / ok 963 / fail 0**；`binary-read-smoke` **exit 0**，manifest 锁定样本 16/16 |
| **逐项语义比对** | `[deep-compare] items=18067 maxResidual=0 mismatches=0`；完整提交 **8/8**、重开一致（骨架 8 / 图集 8 / 纹理 8 / 区域 243 / 骨骼 331）；region 未命中 **0** |
| `skeleton-import-smoke` | **exit 7** = 共享图集布局的已知缺口（2 个单位，Task 6）—— **不作验收证据、未包装成通过** |

交付边界恪守「**结构可读**」：只声称解析 / 建模 / 持久化 / 重开逐项一致，
**未**声称可渲染、可蒙皮、可播放动画、可求值 IK / transform。

---

## 3. 关键决策（已写入 `DECISIONS.md`）

| 编号 | 要点 |
|---|---|
| D29 | 第二批直接增量改结构 + 升 v3，不做 capability 协商层 |
| D30 | 权重**保留源原值**，只校验和；归一化归求值侧 |
| D31 | `hull` / `edges` **不落 canonical**（独立 runtime、不回写 Spine） |
| D32 | BoundingBox（语料 0 例）只留占位；**校验 / 实现延后至 Qt GUI 阶段** |
| D33 | 共享图集布局**不并入 Task 5**，另立 Task 6 |
| D34 | 单条负权重合法（判据是权重和） |
| D35 | IK `enabled` 由 `mix` 判定 |
| D36 | transform 的 mix / offset **原值承载**，D36 不解释语义 |
| D37 | 权重和必须**有限且 > 0** |
| D38 | linked 的两条路径（显式 skin / `parentSkin = null`）都要支持，fixture 08 / 14 |
| D39 | V3 时基**默认 60 FPS**（存储用整数 tick） |
| D40 | 共享图集**复用同一份 Atlas / Texture 资产**（Task 6） |
| D41 | 仅逆向理解 Spine 类型，无源码 / 实现挪用、非分发 |
| D42 | 稳定顶点 ID / 索引契约的工程零点参考 = (0,0,0) |

另于 2026-10-24 按负责人裁定把 `sourceVersion` **统一为 kV3**（默认 + 两处插件赋值 + 1 处既有断言同步）。

---

## 4. 遗留项与交接

| 项 | 去向 | 说明 |
|---|---|---|
| **共享图集 2 个单位**（`h0164s5/watch/`） | **Task 6** | D33 立项、D40 已定资产语义；修好后 smoke 才具备回到 **0** 的条件 |
| **BoundingBox** | D32 → **Qt GUI 阶段** | 语料 0 例，未实现、未验证；不得写成「已支持」 |
| **动画 / 时间线 / 事件 / deform** | **Phase 3**（另立任务） | V3 时基已由 D39 定案（默认 60 FPS），解除阻塞但实现不属 Task 5 |
| **类型覆盖台账** | `Agent_Task/Type_Coverage_Status.md` | 持续维护，问「还有哪些没实现」先看它 |
| **工作区未提交** | 负责人手动提交 | B1/B2 返修 + B3 + B4 + B5 叠加栈，停在 `049b1d3` |

---

## 5. 过程中的重要教训（供后续 Task 复用）

1. **`Status::ok()` 是静态工厂**（`diagnostics.hpp:129`），判成功必须用 `isOk()`。
   写成 `!status.ok()` ⇒ 分支恒为 false ⇒ **静默吞掉全部校验失败**（插件与图集各中招一次）。
2. **codec 对未知字段做字面量回写** ⇒ 编码侧新增键时必须同步解码侧「已知键集合」，
   否则重复键、第二次保存后打不开；验证必须做 **≥2 次保存的多轮往返**。
3. **补断言才能发现真缺陷**：本 Task 的 P0 全部是「新增断言」逼出来的。
   修「死代码类」缺陷后务必做**反向探针**（临时还原缺陷 → 确认断言变红 → 再还原）证明断言有效。
4. **Spine 3.2.01 字节层**：多字节数值是**大端**；`count()` 必须带护栏；
   region（类型 0）**没有前置 color**，mesh 族才有。
5. **环境**：`c++.exe` 已不存在（用 `g++.exe`），旧构建缓存全部失效；
   PowerShell 5.1 会把 BOM-less UTF-8 脚本按 ANSI 读 ⇒ 后台脚本一律纯 ASCII。
6. **验收纪律**：实现报告里**没留日志**的关键项必须自行复跑（本次 B5 的双口径就是）；
   「逐项比对」类新能力要读比对函数确认真逐字段，而非只比数量。

---

## 6. 归档后的下一步

- Task 5 **归档**，不再新增改动。
- 下一个候选：**Task 6（共享图集布局的导入支持）**，阶段 A 方案见
  `Agent_Task/Task6_Implementation_Plan.md`，**待负责人批准后**方可开工（阶段 A 只取证与提案，不实现）。
