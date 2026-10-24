# Task 5 阶段 B：B3 返修报告（第二轮，对应 `Task5_B3_Review.md`）

- 上游：`Agent_Task/Task5_B3_Review.md`（复核结论：**不通过**，2 个 P0 + P1/P2/P3）、`Agent_Task/Task5_B3_Report.md`（第一轮）
- 日期：2026-10-22
- 状态：**P0-1 / P0-2 / P1 / P2 已闭合**；B4 / B5 未开工
- 停点：按计划 §9 停下等待复核

## 1.1 第一轮已交付的产品代码（保留记录）

| 文件 | 改动 |
|---|---|
| `importers/spine32/include/.../source_model.hpp` | 新增源 IR：`SourceMeshAttachment` / `SourceVertexBinding` / `SourceWeightedMeshAttachment` / `SourceLinkedMeshAttachment` / `SourceIkConstraint` / `SourceTransformConstraint`；`SourceAttachment` 增三个可选载荷；`SourceSkeleton` 增 `ikConstraints` / `transformConstraints` |
| `importers/spine32/src/binary_reader.cpp` | mesh 族由「读完即弃」改为**真实解析**（uvs / triangles / vertices / 绑定权重 / linked 父引用）；骨架级约束**真实解析**；`hull` / `edges` 只过字节（D31）；boundingbox 只计数占位（D32） |
| `importers/spine32/src/mapper.cpp` | 第二批附件 → canonical（`MeshAttachment` / `WeightedMeshAttachment` / `LinkedMeshAttachment`）；约束 → `IkConstraint`（`enabled = mix != 0`，D35）/ `TransformConstraint`（原值，D36）；linked 父引用 `(slotId, attachmentName)`、不复制几何；region 查找沿用 `path ?? name` + 尾随空格降级 |

第一轮的 3 处既有断言改动（`MeshAttachmentTraversed` / `WeightedMeshAttachmentTraversed` 由「只计数」改为断言真实解析结果；
mapper 的「非 region 附件只计数不建对象」改为新语义）已在第二轮的 §3 中一并列出。

## 2. 返修清单对照

| 复核条目 | 状态 | 落地 |
|---|---|---|
| **P0-1** 16 个 fixture 接入产品测试、逐条断言 golden | ✅ | 16 个 fixture **全部**接入 `loongbones_binary_read_tests`（字节层）与 `loongbones_spine32_mapper_tests`（IR → canonical → Domain 拒绝）；fixture 目录由 CMake 在构建时注入（`LOONGBONES_FIXTURE_DIR`），测试不依赖当前工作目录 |
| **P0-2** 新增断言达计划下限（binary_read ≥ 20、mapper ≥ 15） | ✅ | binary_read **27 → 47** 个测试（+20）；mapper **17 → 32** 个测试（+15） |
| **P1** D38 两阶段偏离需 fixture 08 / 14 验证 | ✅ | 08（类型 4 + 显式 `parentSkin=default`）与 14（类型 5 + `parentSkin=null`）**双向**在产品 Reader 与 Mapper 上验证通过 |
| **P2** 拒绝路径无产品级断言 | ✅ | 09 / 10 / 12 / 13 四条拒绝路径在 **Domain** 层断言**具体诊断码**（`MESH_TRIANGLE_INDEX_OUT_OF_RANGE` / `MESH_WEIGHT_BONE_UNRESOLVED` / `MESH_WEIGHT_SUM_INVALID` ×2） |
| **P3** import 指标未独立复跑 | ✅ | 重跑骨架导入 smoke（语料 1113），见 §4 |

## 2. 接入 fixture 过程中查出的**两个真实缺陷**（复核未列，属返修的附带产出）

### 2.1 `bendPositive` 的字节语义：语料用 **0xFF 表示 false**

- 现象：fixture 15（`mix = 0` ⇒ `enabled = false`，`bendPositive = false`）在产品 Reader 下读成 **true**。
- 取证：新增只读探针 `tools/skeleton-probe/ik-bendpositive-probe.mjs`（**直读原始字节**，绕过旧参照 Reader），
  语料 1113/1113 解析成功、IK 约束 2707 条 ⇒ 字节分布 **`0x01` × 1348 / `0xFF` × 1359**。
  同一普查确认 `nonessential`（0x00×1109 / 0x01×4）、`inheritScale`、`inheritRotation` 都是**标准 0/1**。
- 结论：3.2.01 的 `bendPositive` **必须按有符号判定**（`>= 0` 为 true）。
  官方 Spine 3.x 的 `readBoolean()`（`!= 0`）会把 1359 个 false 读成 true ⇒ **不得使用**。
  旧参照 Reader 的 `readByte() >= 0` 在这一点上是对的。
- 修正：`binary_reader.cpp` 的 IK 段改用有符号判定；其它 bool 保持 `!= 0`（有证据支撑）。
  **未改任何 fixture 字节** —— fixture 15 原本就写的 0xFF，与语料一致。

### 2.2 linked 附件的 region 来自**父附件**，不是自己

- 现象：fixture 08 的 linked 附件 region 查不到 —— 它的名字是 `body_mesh_linked`，
  而它**没有自己的贴图**，贴图来自父附件 `body_mesh`。
- 修正：mapper 在同一 slot 内**先建「附件名 → regionId」表**，再映射 linked
  （父的 region 优先；父找不到才回退到自身键）。这正是计划里「两阶段」的实际用武之地：
  不是为了解析父几何，而是为了解析**父的 region**。
- 副作用修正：预扫**不产诊断**（尾随空格降级 warning 仍由附件自己的映射路径产出一次），
  否则同一附件会被报两次（既有断言 `countSeverity == 1` 会回归）。

## 3. 既有断言的改动（逐条披露）

| # | 位置 | 改动 |
|---|---|---|
| 1 | `test_mapper.cpp` "mesh attachments are no longer skipped…" | 断言从「未命中即 error」改为「映射层不因 region 未命中而失败」（未命中交 Domain 判），否则会抢先掩盖几何 / 权重诊断码 |
| 2 | `test_mapper.cpp` `meshAttachment()` helper | 改为产生**带真实载荷**的 mesh 附件（原先只有 typeByte），与 Reader 的真实 IR 一致 |
| 3 | `test_mapper.cpp` "fixture 15 maps ik with mix zero to disabled" | 断言 `!bendPositive` —— 由 §2.1 的修正支撑（返修前该断言失败，正是缺陷暴露点） |

## 4. 验证

| 项 | 结果 |
|---|---|
| 干净构建 `build_b1b2` | cmake 0 / make **0**，**零 error、零 warning** |
| ctest | **7 / 7**（100%） |
| `loongbones_binary_read_tests` | **47 / 47**（基线 27 → **+20** 个测试） |
| `loongbones_spine32_mapper_tests` | **32 / 32**（基线 17 → **+15** 个测试） |
| core / format / skeleton_import / atlas / probe | 零回归（core 135、format 86、skeleton_import 27、atlas 8） |
| 骨架导入 smoke（语料 1113） | 见 §5；退出码 7 = 已知共享图集布局缺口（Task 6），非本次引入 |

## 5. 语料闭环（smoke）

- 语料 1113 → prepare 成功 1111、失败 2（已知布局缺口）、**完整提交（含落盘 v3 → 重开）8 / 8**、region 未命中 0。
- **不支持能力清单里第二批条目全部消失**（此前 `mesh 2979` / `weightedMesh 17271` / `weightedLinkedMesh 2` /
  `ikConstraints 2707` / `transformConstraints 208` 均在清单中）；`IMPORT_UNSUPPORTED_CAPABILITY` 9463 → **6636**。
- 退出码 **7** = 已知共享图集布局缺口（2 个单位，Task 6）—— **不包装成通过**。

## 6. 新增工具登记

`tools/skeleton-probe/ik-bendpositive-probe.mjs`（只读、只输出统计量与字节分布，不落地任何游戏资产）
已登记进 `tools/skeleton-probe/README.md`；输出 `tools/skeleton-probe/out/ik-bendpositive-probe.json`。

## 7. 未完项

| 项 | 说明 |
|---|---|
| **B4** | 插件诊断与报告的**显式**改造未做（账本条目因第二批已支持而**自然**消失，但计划要求「boundingbox 仍记待验证」等显式处理） |
| **B5** | 语料闭环的**双口径**（Full 1113 / Primary 963）与「≥3 个真实样本 Spine→Domain→Native→reopen→逐项比对」未做；失败原子性（注入失败验证快照一致）未做 |
| **既有断言的两处小盲区** | 复核 §7 顺带提到的 `encoder_always_writes_v2…` 与 `sameAttachment` 兜底分支未补 |
| **提交** | B1/B2 返修 + B3 全部改动**仍未提交**（工作区脏文件） |

## 8. 停点

P0/P1/P2 已闭合、回归绿、语料闭环通过；**B4 / B5 未开工**。按计划 §9「通过也不自动授权 B4」，此处停下。
