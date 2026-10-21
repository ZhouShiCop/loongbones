# Task 5 · 阶段 A 验收（A6）

- 验收日期：2026-10-22
- 验收对象：`Agent_Task/Task5.md` §3 的 A1–A5 全部交付物 + A6 停点纪律
- 验收角色边界（负责人 2026-10-22 明确）：**本对话负责检查、验收与计划，不承担实现**。
  阶段 B 的实现另由实现会话执行；本文件只给出验收结论、开工前置与工作包计划，**不产出代码**。
- **结论：阶段 A 验收通过** —— A1–A5 交付物齐全、内部一致性 5 项全过、停点纪律未被破坏；
  附 **1 项开工前置（R1）** 与 **一批不阻塞的未决项**。**阶段 B 仍需负责人批准后才开工。**

---

## 1. 验收方法

1. 交付物存在性与登记核对（文件、README、`STRUCTURE.md`、`fixtures.json` 计数）。
2. **停点纪律**：`git status` + `git diff` 确认阶段 A 期间未新增 Domain 类型、未改 `format/`
   已冻结字段、未开始插件第二批实现。
3. **内部一致性**：A2 提案 ↔ A1 取证结论、A3 提案 ↔ v2 冻结稿 ↔ D29、fixture 计数 ↔ 登记。
4. **未决项盘点**：区分「不阻塞阶段 B」与「必须在阶段 B 内验证」。

---

## 2. 交付物核对

| # | 应有交付 | 实有 | 结论 |
|---|---|---|---|
| A1 | 取证报告 + 证据文档 + 探针工具 | `Task5_A1_Report.md`、`docs/skeleton/mesh-constraint-evidence.md`；`mesh-constraint-probe.mjs` / `tc1-space-control.mjs` / `type-byte-census.mjs` / `weight-sum-census.mjs` | ✅（Full 1113 / Primary 963，解析失败 0） |
| A2 | canonical 模型提案（6 类：mesh / weightedMesh / linkedMesh / boundingbox / IK / transform） | `docs/skeleton/mesh-and-constraint-rfc.md` | ✅ |
| A3 | 原生格式 v3 增量提案（D29） | `docs/format/native-project-v3-mesh.md` | ✅ |
| A4 | 支持矩阵 / 诊断 / 决策同步 | `field-mapping.md` §11.2、`skeleton-domain-rfc.md` §10.1、`DataType_Report.md` §3、`DECISIONS.md` **D29–D38**、`项目规范.md` §5 | ✅（F7 已关闭） |
| A5 | 合成 fixture ≥ 4，含 TBD-F 硬要求的类型 4 | **16 个**（5 个首批 + 11 个第二批），`08` = 类型 4 / `14` = 类型 5 | ✅（golden 逐值对拍，`Task5_A5_Review.md`） |
| A6 | 交付报告 | `Agent_Task/Task5_A_Report.md` + 本验收记录 | ✅ |

取证关闭情况：`Task5.md` §2.1 —— **T-C1…T-C5 全部关闭**；
未决 TBD 汇总见 §5。

---

## 3. 停点纪律核查

| 项 | 结果 |
|---|---|
| 新增 Domain 类型 | **无** ✅ |
| 修改 `format/` 已冻结字段 | **无** ✅ |
| 开始插件第二批实现 | **无** ✅ |
| 产品代码改动 | **仅 2 个文件 69 行**：`spine32_import_plugin.cpp`（+11/−1，图集诊断码映射）+ `test_spine32_plugin.cpp`（+59，B6 返修的两条断言）。`git diff` 全文检索第二批关键词（mesh / weight / ik_ / transform / vertexBind / constraint）**零命中** ⇒ 属 **Task 4 B6 遗留**，不是本阶段越界 ✅ |

### R1（开工前置，非缺陷）

上述 B6 改动**至今未提交**（工作区已脏：`git log` 最新提交仍是 2026-10-21 的 MVP initial commit）。
阶段 B 开工前必须先做一次**基线提交**把 B6 落定，否则：

- 阶段 B 的新增改动会与 B6 返修混在同一个工作区，出问题无法二分定位；
- B6 已复核通过但**没有可回溯基线**，验收证据链断一截。

---

## 4. 内部一致性核查（5 项全过）

| # | 检查 | 结果 |
|---|---|---|
| C1 | A2 顶点空间 ↔ A1 更正结论（**N2 维持：骨骼局部坐标**） | ✅ 提案 §4 / §5 写「槽骨骼局部坐标」「每条 (bone,x,y) 是**该骨骼自己的**局部坐标」，与 `DECISIONS.md` N2 一致；未出现「骨架空间」残留 |
| C2 | A3 错误码 ↔ v2 冻结稿 §6.1 | ✅ v3 §6.1 第 3 步已按冻结稿改为 `kFormatTooNew` 并留修订痕迹；第 2 步 v1/v2 读 v3 固定 `kFormatVersionUnsupported` |
| C3 | A3 ↔ **D29**（升 v3、不新增 capability） | ✅ v3 §2 / §6 显式声明 `capabilities.required` 沿用 v2（atlas.v1 + skeleton.v1）、**不引入** `loongbones.mesh.v1` |
| C4 | fixture 计数一致性 | ✅ 目录 `.skel.bytes` **16** = `fixtures.json` **16** = `STRUCTURE.md` §2.4 登记 **16**；README 清单与 golden 表同步 |
| C5 | 支持矩阵迁移 | ✅ `field-mapping.md` §11.2 与 `skeleton-domain-rfc.md` §10.1 均已把第二批标为「实现范围（第二批，Task 5）」，旧条目加删除线并注明迁移去向；path / clipping 维持「不支持且有诊断」 |

---

## 5. 未决项盘点

### 5.1 不阻塞阶段 B（可在 B 之后或并行处理）

| 项 | 内容 |
|---|---|
| TBD-A | `uvs` 的 V 方向（V1）—— 提案不声明方向，只承载原值 |
| TBD-B | UV 越界无真实样本（语料 0 例），拒绝策略无法用真实语料验证 |
| TBD-C | transform 的 mix / offset 真实语义 —— 被 **V3 时基**阻塞，D36 已裁定「不解释语义」 |
| TBD-D | LinkedMesh 的 L0 / L1 导出策略 —— 归导出任务 |
| TBD-E | region / 附件计数 47,705 / 47,709 / 47,711 的差 4 —— 仅影响文档数字，不影响建模与实现 |
| V3（动画时基） | 阻塞 deform / 动画建模与 transform 语义解释；**不阻塞**本批静态数据 |

### 5.2 阶段 B 内必须验证（fixture 已有、行为未验证）

| 项 | 说明 |
|---|---|
| 08 / 14 的父引用解析 | 旧 Reader 找不到父是其实现局限；**产品 Reader 必须能解析**，`parentKey` 两条路径（显式 `parentSkin` / `null` 隐式）都要覆盖 |
| 09 / 10 / 12 / 13 的拒绝行为 | fixture 只提供字节；产品必须拒绝（`MESH_TRIANGLE_INDEX_OUT_OF_RANGE` / `MESH_WEIGHT_BONE_UNRESOLVED` / `MESH_WEIGHT_SUM_INVALID`） |
| D37 拒绝路径无真实样本 | 语料 1113/1113、加权顶点 312,083 中 `sum ≤ 0` 与非有限**均 0 例** ⇒ 不得称「语料已验证」 |
| D32 的 boundingbox | 只留占位，遇到须出诊断、不建对象；语料 0 例，支持矩阵维持「待验证」 |

---

## 6. 阶段 B 开工前置（checklist）

- [ ] **R1**：B6 遗留改动建立基线提交（含 `spine32_import_plugin.cpp` / `test_spine32_plugin.cpp` 及其测试）。
- [ ] 负责人批准进入阶段 B（阶段 A 验收通过 ≠ 自动授权实现）。
- [ ] 实现会话明确沿用：D29–D38、`docs/skeleton/mesh-and-constraint-rfc.md`、`docs/format/native-project-v3-mesh.md`、
      `docs/skeleton/fixtures/` 的 16 个 fixture 与 README 的 golden 表。
- [ ] 记住两个已踩过的坑：`ProjectData` / `ProjectDocument` 布局变更后必须**全量重编**（增量链接会 ODR 违例）；
      语料为**只读**，smoke 退出码只有 **0** 可作验收证据。

---

## 7. 阶段 B 工作包计划（本对话不执行）

按 `Task5.md` §4 的 B1–B5，规模估算（按代码量，非日历承诺）：

| 包 | 内容 | 量级 |
|---|---|---|
| B1 | Domain 新增 5 个类型 + 校验，诊断码只追加，core 116 / format 70 不回归 | 中 |
| B2 | `migrateV2ToV3()` 纯函数 + v1/v2/v3 三读 + 统一写 v3 + 锚点 | 中（参照 v1→v2 迁移） |
| B3 | **最大**：Reader 落地 6 类附件（现只 region，mesh 是 skip）+ IK / transform（现只计数）+ Mapper | 大（现 `binary_reader.cpp` 899 行 / `mapper.cpp` 294 行） |
| B4 | 插件诊断码与 `report.unsupported` 账本 | 小 |
| B5 | Full 1113 / Primary 963 语料闭环 + ≥3 个真实样本往返比对 | 中 |

- 合计约 **1,400–2,000 行产品代码 + 1,400–1,800 行测试**。
- 固定成本：全量编译约 **20 分钟**，增量 1–3 分钟/次。
- 交付边界沿用 Task 4：**「结构可读」**——不得把「可导入 / 可解析」表述为「可渲染 / 可蒙皮 / 可播放 / 可求值」。

---

## 8. 验收结论与停点

**阶段 A（A1–A6）验收通过。** A6 停点要求「获批前禁止新增 Domain 类型、修改已冻结字段、开始插件实现」
—— 经 `git diff` 逐项确认**未被破坏**，工作区里唯一的产品代码改动是 Task 4 B6 的遗留返修。

按任务节奏：**一个工作包批准不自动授权下一个**。阶段 B 由负责人批准后，交由实现会话推进；
本对话在 B1–B5 完成后继续承担验收。
