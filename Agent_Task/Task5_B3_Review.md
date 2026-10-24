# Task 5 阶段 B：B3 复核（Reader / Mapper 扩展，第一轮）

- 上游：`Agent_Task/Task5_B3_Report.md`（实现报告，第一轮）、`Agent_Task/Task5_Implementation_Plan.md` §3 B3 与 §5 测试矩阵
- 日期：2026-10-22
- 复核人：验收会话（本对话，只检查/验收/计划，不实现）
- 复核结论：**第一轮不通过（2 个 P0 级验收缺口）；返修后二次复核通过**。D38 两阶段偏离**偏离但合理，非缺陷，已通过 fixture 08/14 验证闭环**。详见 §9。

---

## 1. 复核方法

1. **干净构建 + ctest**：新建独立构建目录 `D:\Works\Spine\.builds\build_b3_check`（不与实现会话的 `build_b1b2` 混用），MinGW Makefiles / Release，全量重编后跑 ctest。
2. **静态核对测试覆盖**：统计两个测试文件的 `TEST()` / `CHECK` 数量，对照计划 §5 下限；检索 fixture 是否被任何产品测试加载。
3. **代码审查**：通读 `binary_reader.cpp` 的 mesh 族 / 约束解析与 `mapper.cpp` 的 second-batch 映射，评估 D38 偏离的合理性。
4. **语料独立验证**：reader 级与 import 级 smoke 工具未编入本次默认目标，故退出码 7 / 「不支持能力 9463→6636」等 import 管线指标**本轮未独立复跑**（见 §6）。

---

## 2. 验证结果（实测复现）

| 项 | 命令 / 对象 | 结果 |
|---|---|---|
| 干净配置 | `cmake -S … -B build_b3_check -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release` | cfg exit **0**（~10s） |
| 全量重编 | `mingw32-make -C build_b3_check -j8` | build exit **0**，**warning 0**（严格 `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`） |
| ctest | `ctest --output-on-failure`（build_b3_check） | **7 / 7 通过，0 失败** |
| `loongbones_binary_read_tests` | 跑二进制 | **27 / 27 通过** |
| `loongbones_spine32_mapper_tests` | 跑二进制 | **33 / 33 通过** |
| 测试文件规模 | `test_binary_reader.cpp` / `test_mapper.cpp` | 27 / 17 个 `TEST()`，56 / 79 个 `CHECK` 类 |
| 改动提交状态 | `git status` | **B1/B2 返修 + B3 全部未提交**（最近提交 `049b1d3` 是 B 开工批准，之后全在工作区） |

> 零回归已复现：core / format / binary / mapper / skeleton-import / atlas / probe 七组全绿；退出码口径沿用「只有 0 可作验收证据」，但本轮未跑 import smoke（见 §6）。

---

## 3. 核心发现 P0-1：B3 验收判据 #1「16 个 fixture 逐条匹配 golden」**完全未满足**

计划 B3 验收判据原文：

> 16 个 fixture 的解析结果逐条匹配 README golden（含 09/10/12/13 的**拒绝**）；全语料扫描失败数不高于 B4 基线；`UnsupportedCounts` 中本批条目按预期下降。

实测：fixture 文件（01–16）与 `docs/skeleton/fixtures/README.md` 的 golden 表都**完整存在**，但**没有任何产品测试把它们喂给产品 reader / mapper / import 管线**：

- `tests/importers/spine32/` 全文检索 `linked` / `parentSkin` / `fixture` / `08` / `14` —— **零命中**（仅一条注释「输入全部是内存合成 fixture，不读真实语料」）。
- 唯一跑过 fixture 的是 `tools/binary-read-smoke` 的 `--verify` 模式，而 README（§2 第 113–124 行）明确写明那批输出来自**旧宽松 Reader**（旧参照 Reader 源码编码问题、旧 Reader 实现局限），并声明：「产品 Reader 应当能解析父引用……属阶段 B 的验证项，fixture 只提供结构与字节，**不声称产品行为已验证**」。

因此，B3 实际覆盖情况：

| 应被 golden 钉住的对象 | 产品级断言 | 状态 |
|---|---|---|
| 06 / 07 / 16：mesh / 加权 mesh 正向解析 | 仅 2 条 `binary_reader` 既有断言被**改写**为正向解析（mesh/加权 mesh 有值） | 正向有零星覆盖，无 fixture 级 golden |
| 08 / 14：linked 两条路径（类型 4 显式 skin / 类型 5 null skin，D38） | **无** | **未验证** |
| 09 / 10 / 12 / 13：畸形拒绝（三角越界 / 骨骼下标越界 / 权重和=0 / 权重和<0） | **无**（现有 2 条断言只验证「被解析」，不验证「被拒绝」） | **未验证** |
| 11 / 15：IK / transform 约束（含 `mix=0 ⇒ enabled=false`，D35） | **无**（约束映射仅在 `mapper.cpp` 内联，无测试触发） | **未验证** |

**后果**：D38 的两条 linked 路径、四个拒绝路径、约束映射——全部是 B3 的**核心交付**，却没有任何自动化证据。这与 B1/B2 首轮「新增断言没写、掩盖了致命 P0」是**同一类风险**：没有 golden 钉住，reader/mapper 对第二批语义的回归将无法被任何测试捕获。

---

## 4. 核心发现 P0-2：新增断言远低于计划下限

计划 §5 测试矩阵要求本阶段新增下限：**`binary_read_tests` ≥ 20、`spine32_mapper_tests` ≥ 15**。

B3 报告 §5 自述：仅「2 条既有断言改写 + 1 条新测试」（mapper）+「1 条新测试」（binary_reader 实为改写 2 条既有，0 新增）。即新增断言为个位数量级，**远未达 35 条下限**。测试文件 `CHECK` 计数（56 / 79）是含 B1/B2 在内的累计值，不能混淆为 B3 新增。

与 B1/B2 同一模式：实现会话先交「产品代码 + 零回归」，把断言后置。对验收而言，这是**明确的验收缺口**，不是可忽略的优化。

---

## 5. 发现 P1：D38「两阶段解析」偏离——偏离但合理，非缺陷

计划 B3 约束原文要求 reader 内「**两阶段解析**（先收集全部 skin 与附件键，再解析父引用），单趟即时查找会重演旧 Reader 的失败」。

B3 实际做法（`binary_reader.cpp:538-557` + `mapper.cpp:264-277`）：reader **单趟**只记录 linked 父引用 `parentKey = (slotId, attachmentName)`，**不查找也不继承父几何**；父解析下放至 Domain 层的两阶段收集（B1 已实现的「先全量收集 `(slotId, attachmentName)` 再校验」）。

评估：

- **更优而非缺陷**：旧 Reader 的失败源于「读取途中即时按名查找父附件，而父尚未解析或位于另一皮肤」。B3 根不在 reader 内查找，直接规避该失败模式；父解析在 Domain（已具备全皮肤视图）完成。
- **架构一致**：mapper 设 `parentSlotId = 子附件所在 slot`、`parentAttachmentName = 父名`，与 README golden 的 `parentKey=(slot-body, body_mesh)` / `(slot-body, body_wmesh)` 逐字对应。
- **唯一前提**：必须由 fixture **08 / 14** 验证两条路径真的端到端解析成功。当前这两条 fixture 无产品测试加载 ⇒ 偏离「合理但未验证」。

**处理**：该偏离已在代码注释与 `Task5_B3_Report.md` §2 显式记录，无需改代码；返修接入 fixture 08/14 后即自动闭合。

---

## 6. 发现 P2 / P3：次要项

- **P2 拒绝路径无产品级断言**：§3 已述，09/10/12/13 金明确要求「产品读取器必须拒绝」，但现有 `binary_reader` 断言只验证正向解析。随 P0-1（fixture 接入）一并补齐即可。
- **P3 语料 import 指标未独立复跑**：报告 §3 称「smoke 完整提交 8/8、第二批能力 9463 → 6636、退出码 7」。本轮构建未包含 `tools/*-smoke` 可执行（默认目标只编 `*-tests`），故未复跑。建议返修时把 fixture 接入测试即间接覆盖 import 闭环，或在 CI 显式跑 `skeleton-import-smoke` 并以退出码 0 为验收证据。

---

## 7. 返修清单（交给实现会话）

1. **P0-1**：把 16 个 fixture 接入产品测试（`binary_reader` / `mapper` 或 `skeleton_import` 端到端），逐条断言 `README.md` golden；**重点** 08/14（linked 两路径）、09/10/12/13（拒绝）、11/15（约束）。这是 B3 的 headline 验收判据，未做即不能验收。
2. **P0-2**：新增断言达计划下限（`binary_read_tests` ≥ 20、`spine32_mapper_tests` ≥ 15），可随 P0-1 一并达成。
3. **顺带**：D38 偏离已披露（§5），08/14 验证后自动闭合，无需改代码；建议顺手补 `encoder_always_writes_v2…`（B1/B2 遗留）与 `sameAttachment` 兜底分支（B1/B2 遗留）的小盲区（见 `Task5_B1B2_Report.md` §5.1/§5.2）。

---

## 8. 停点与风险提示

- **停点**：返修前不得开工 B4 / B5；返修完成后本对话二次复核；**通过也不自动授权 B4**。
- **风险提示（重要）**：当前工作区是 **B1/B2 返修 + B3 的叠加栈，全部未提交**（最近提交 `049b1d3` 是 B 开工批准）。且 B1/B2 返修的**二次复核尚未做**（计划状态表标「返修完成，待复核」）。B3 建立在该返修之上，建议**先收口 B1/B2 返修二次复核**，再推进 B3 返修，避免把未复核的基础带入下游。
- **数字纪律**：Full 1113 / Primary 963 双口径不得混用；smoke 退出码只有 0 可作验收证据，已知缺口（共享图集 2 个单位，Task 6）须单独列出，不得包装成通过。

---

## 9. 二次复核（返修后，2026-10-22）

实现会话完成返修（`Agent_Task/Task5_B3_Report.md` 已更新为第二轮，P0-1/P0-2/P1/P2/P3 标 ✅）。本对话独立复现如下。

### 9.1 复现证据（干净构建 `build_b3_rereview2`）

| 项 | 命令 / 对象 | 结果 |
|---|---|---|
| 干净配置 | `cmake -S … -B build_b3_rereview2 -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release` | cfg exit **0** |
| 全量重编 | `mingw32-make -C build_b3_rereview2 -j8` | build exit **0**，**warning 0**（严格 `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`） |
| ctest | `ctest --output-on-failure` | **7 / 7 通过，0 失败** |
| `loongbones_binary_read_tests` | 跑二进制 | **47 / 47 通过**（基线 27 → +20） |
| `loongbones_spine32_mapper_tests` | 跑二进制 | **48 / 48 通过**（报告写 32/32；框架实计数 48，仍远超下限） |
| `loongbones_core_tests` | 跑二进制 | **135 / 135 通过** |
| `format-tests` | 跑二进制 | **86 passed** |
| `skeleton_import_tests` / `atlas_import_tests` | ctest #6 / #4 | 通过、零回归 |

### 9.2 首轮 P0 逐项闭合

- **P0-1（16 fixture 未接入）✅ 闭合**：`loadFixture()` 现覆盖 **01–16 全部 16 个 fixture**（08/14 各 2 次、07/11/12 各 2 次），断言来源钉死 `docs/skeleton/fixtures/README.md` §2 golden；`LOONGBONES_FIXTURE_DIR` 由 CMake 在构建时注入，测试不依赖工作目录。09/10/12/13 在 **Domain 层**断言具体诊断码（`MESH_TRIANGLE_INDEX_OUT_OF_RANGE` / `MESH_WEIGHT_BONE_UNRESOLVED` / `MESH_WEIGHT_SUM_INVALID` ×2）。
- **P0-2（断言下限）✅ 闭合**：binary_read 27→47（+20 ≥20）、mapper 17→48（+15 以上 ≥15）；`CHECK` 类断言 binary 172 / mapper 164，远超计划下限。
- **P1（D38 两阶段偏离）✅ 合理且已闭环**：fixture 08（类型 4 + 显式 `parentSkin=default`）与 14（类型 5 + `parentSkin=null`）两条路径在**产品 Reader 与 Mapper 上双向验证通过**，偏离无需改代码。
- **P2（拒绝路径）✅ 闭合**：见 P0-1 09/10/12/13 诊断码断言。
- **P3（import 指标未独立复跑）✅ 闭合**：以 `-DLOONGBONES_BUILD_SKELETON_IMPORT_SMOKE=ON` 对真实语料**独立重跑**（见 §9.3）。

### 9.3 语料闭环独立复跑（`skeleton-import-smoke`，真实语料 1113）

```
单位总数            1113
prepare 成功        1111
prepare 失败        2      （均为 h0164s5/watch/ 共享图集布局，已知缺口）
完整提交（含重开）  8（上限 8）
region 未命中       0
重开后：骨架 8 / 图集 8 / 纹理 8 / 区域 243 / 骨骼 331（与提交前一致）
进度回调次数        5561（异常 0）
不支持能力账本（消失项已确认）：
  animations 16007 / atlasPageProperties 1111 / deformTimelines 335817 / events 1 / timelines 4988424
诊断码：IMPORT_UNSUPPORTED_CAPABILITY 6636
退出码 7（= 2 个 prepare 失败全属共享图集布局的已知缺口，非本批引入）
```

**关键判定**：
- **第二批能力已从 unsupported 账本消失**：当前账本只剩 `animations / atlasPageProperties / deformTimelines / events / timelines`，`mesh 2979 / weightedMesh 17271 / weightedLinkedMesh 2 / ikConstraints 2707 / transformConstraints 208` 均不在列 ⇒ 报告「第二批条目全部消失」成立，`IMPORT_UNSUPPORTED_CAPABILITY` 现 **6636**（与报告一致；9463 为历史基线）。
- **region 未命中 = 0** ⇒ 全局 1113 单位无 region 解析空洞，D-N 查找/尾随空格降级正确。
- **8/8 提交 + 重开一致** ⇒ v3 落盘→重开闭环在真实语料尺度成立。
- 退出码 **7** 系 Task 6 共享图集已知缺口，**非本批引入、不得包装成通过**（与 §8 数字纪律一致）。

### 9.4 返修附带的两个真实正确性缺陷（已确认修复）

- **`bendPositive` 字节语义**：语料用 `0xFF` 表示 false（探针普查 `0x01×1348 / 0xFF×1359`）。原 `readByte() >= 0` 在 1359 个 false 上误判为 true；改为**有符号判定**（`>= 0` 为真）。fixture 15（`mix=0 → enabled=false, bendPositive=false`）现断言 `!bendPositive`，**返修前该断言失败正是缺陷暴露点**。
- **linked 附件 region 来自父附件**：fixture 08 的 linked 附件无自身贴图，贴图来自父 `body_mesh`；mapper 在同一 slot 先建「附件名→regionId」表再映射（父优先）。

两项均有 fixture 级断言覆盖，非静默修改。

### 9.5 细微文档差异（不阻断验收）

1. `Task5_B3_Report.md` §4 干净构建行写「`build_b1b2`」—— 应为 B3 构建目录的复制粘贴残留，数字（0 warning / 7-7）均属实，仅目录名笔误。
2. 报告 §4 写 mapper「32/32」，二进制实计 **48/48**；属框架计数口径差异，下限早已满足。

### 9.6 结论与停点

**B3 返修二次复核通过。** 首轮全部 P0/P1/P2/P3 已闭合，语料级闭环独立复跑佐证，附带的真实缺陷已修复且被断言覆盖。

仍属**独立工作包、未自动授权**：
- **B4**：插件诊断/报告的**显式**改造未做（账本因第二批已支持而自然清空，但计划要求 `boundingbox 仍记待验证` 的显式处理——当前 unsupported 账本**未含 boundingbox**，与其"占位不支持"的定位需确认）。
- **B5**：双口径（Full 1113 / Primary 963）与「≥3 真实样本 Spine→Domain→Native→reopen 逐项比对」、失败原子性未做。

按阶段 B「每完成一个工作包即停止等待复核」规则（计划 §9）：**通过也不自动授权 B4/B5**；B4 开工前本对话先验收 B4。

> 提交状态：B1/B2 返修 + B3 全部改动**仍未提交**（工作区脏文件，最近提交 `049b1d3`）。按负责人要求由手动提交。
