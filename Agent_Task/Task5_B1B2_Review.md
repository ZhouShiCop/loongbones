# Task 5 阶段 B：B1 + B2 复核记录

- 复核日期：2026-10-22
- 复核对象：`Agent_Task/Task5_B1B2_Report.md` + 工作区未提交的 B1/B2 改动（17 个文件，+925/−70）
- 方法：① 干净构建目录**全量重编 + 全量 ctest**（不复用 `build_b1b2`）；② 逐个测试目标取断言计数；
  ③ 逐条对照 `Agent_Task/Task5_Implementation_Plan.md` §4 的 B1 / B2 验收判据与 §5 测试矩阵；
  ④ 对「未知字段 round-trip」这一硬判据写临时程序**实测**；⑤ 纪律核查
- **首轮结论：不通过**（2026-10-22）：存在 1 个阻断性缺陷（P0），v3 文档保存后会损坏
- **二次复核结论：通过**（2026-10-22 晚）：返修已完成并独立验证，P0 已修复且有多轮往返断言覆盖；详见 §7。B1/B2 验收解除，解锁 B3（B3 自身返修另计）

---

## 1. 复现结果（与报告一致的部分）

```text
cmake -S . -B ..\.builds\build_b1b2_verify -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -C ..\.builds\build_b1b2_verify -j8      # exit 0，1m50s，warning 计数 = 0
ctest --test-dir ..\.builds\build_b1b2_verify         # 7/7 通过
```

| 目标 | 报告声称 | 实测 | |
|---|---|---|
| `core_tests` | 116 | **116 / 116** | ✅ |
| `format_tests` | 70 | **70 / 70** | ✅ |
| `binary_read_tests` | exit 0 | **27 / 27** | ✅ |
| `spine32_mapper_tests` | exit 0 | **32 / 32** | ✅ |
| `atlas_import_tests` | exit 0 | **8 / 8** | ✅ |
| `skeleton_import_tests` | exit 0 | **27 / 27** | ✅ |
| `format_probe_selftest` | exit 0 | 通过 | ✅ |
| 编译 warning | 0 | **0** | ✅ |

**这些数字全部复现**。但它们是**既有基线**，本轮新增断言为 **0**（见 §4）。

---

## 2. 通过项（逐条对照计划 §4）

### B1

| 判据 | 结果 | 依据 |
|---|---|---|
| 5 个新类型 + boundingbox 占位 | ✅ | `skeleton.hpp`：`MeshAttachment` / `VertexBinding` / `WeightedMeshAttachment` / `LinkedMeshAttachment` / `Attachment`（variant）/ `IkConstraint` / `TransformConstraint`；`BoundingBoxAttachment {}` **空结构体且不进 variant**（D32） |
| 附件内嵌不单独分配 ID；约束有稳定 ID | ✅ | `Attachment` 是 `SkinEntry::attachment` 的值对象；`IkConstraintId` / `TransformConstraintId` 由宿主生成 |
| **只增不改** | ✅ | `RegionAttachment` 字段与语义未动；`IdKind` 两个新值**末尾追加**；9 个诊断码**末尾追加** |
| 顶点空间 N2 维持 | ✅ | 类型注释明确「非加权 = 槽骨骼局部、加权每条 = 该骨骼局部」，无「骨架空间」残留 |
| 权重保留原值、校验和 > 0、单条负权重合法 | ✅ | `checkWeightedMesh()`：`!(sum > 0.0)` ⇒ `kMeshWeightSumInvalid`；判据在**和**、不看单条符号（D34 + D37） |
| `hull` / `edges` 不落 | ✅ | 两个类型都没有这两个字段（D31） |
| D38 linked 两阶段解析 | ✅ | `checkSkins()` 先全量收集 `(slotId, attachmentName)` 键集合，再逐条校验父引用 |
| D36 约束不解释语义 | ✅ | `checkConstraints()` 只校验 ID 形态/种类、骨骼闭合、链非空、数值有限 |
| core 116 零回归 | ✅ | 实测 |

### B2

| 判据 | 结果 | 依据 |
|---|---|---|
| `kFormatVersion` / `kReaderMaxVersion` → 3、`kReaderMinVersion` = 1 | ✅ | `codec.hpp` |
| `migrateV2ToV3()` 纯函数、幂等 | ✅ | 只改 `sourceVersion`，不生成 ID、不改字段、不碰磁盘；`sourceVersion != kV2` 直接返回 |
| 迁移链 v1→v2→v3 串联 | ✅ | `decodeDocument()` 内依次调用 |
| 统一写 v3 | ✅ | 写出 `formatVersion: 3` / `minimumReaderVersion: 3` |
| 两个约束键**恒存在** | ✅ | `emitSkeleton()` 无条件写两个数组（可为空） |
| `OwnerKind` 新增两类 | ✅ | `kIkConstraint` / `kTransformConstraint` |
| region 路径逐字未改 | ✅ | diff 中 region 的编解码主体**无删除行**，只改了函数签名与分派 |
| 未知附件类型显式拒绝、不降级 | ✅ | `decodeAttachment()` 四个 `type` 分支后 `fail(kJsonTypeMismatch)` |
| v2 fixture 保留 | ✅ | `docs/skeleton/fixtures/` 16 个文件未改动 |
| v1/v2 读入不受影响；超上限版本拒绝 | ✅ | `formatVersion=4` 被拒（`kFormatVersionUnsupported`） |

---

## 3. P0（阻断）：v3 文档保存后会因**重复键**而打不开

### 3.1 实测

临时程序（不入库）：构造文档 → `decodeDocument` → `encodeDocument` → 再 `decodeDocument` …… 循环 4 轮。

```text
===== 场景 A：v2 风格文档（region 附件、无约束键，输入 formatVersion=2）=====
  第 1 次打开 ok；保存后："ikConstraints"=1 次
  第 2 次打开 ok；保存后："ikConstraints"=2 次        ← 重复键已产生
  第 3 次打开：失败 src=1 code=4
    loc=/project/skeletons/0/ikConstraints
    msg=对象内出现重复 key：ikConstraints

===== 场景 B：v3 文档（mesh 附件 + 约束，输入 formatVersion=3）=====
  第 1 次打开 ok；保存后："ikConstraints"=2、"uvs"=2、"triangles"=2、"vertices"=2
  第 2 次打开：失败 src=1 code=4
    loc=/project/skeletons/0/skins/0/entries/0/attachment/uvs
    msg=对象内出现重复 key：uvs
```

`code=4` = `kJsonDuplicateKey`。

**影响链（场景 A 是最严重的）**：一个**完全没有第二批数据**的既有工程 ——
第 1 次打开 → 保存（正常）→ 第 2 次打开 → 保存（**产生重复键**）→ **第 3 次打开失败**。
即 **B2 之后，任何工程第二次保存后即损坏**；含第二批数据的工程**第一次保存后即损坏**。
这是相对 v2 的**回归**，且直接违反 B2 判据「v3 文档 encode→decode→encode 语义往返」。

### 3.2 根因（三处，全在 `format/src/decode.cpp`）

编码侧新增了字段与对象的写出，但**解码侧的「已知键集合」没有同步**，于是这些键在读入时被
`collectUnknown()` 当作未知字段收集，保存时再以原始字面量**回写一次**，与结构化写出叠加成重复键：

| # | 位置 | 缺失 |
|---|---|---|
| 1 | `kSkeletonKeys`（:49–51） | 未加 `ikConstraints` / `transformConstraints` |
| 2 | `kAttachmentKeys`（:62–64） | 未加 `uvs` / `triangles` / `vertices` / `vertexBindings` / `parentKey` / `inheritDeform` |
| 3 | `decodeIkConstraint`（:1384）/ `decodeTransformConstraint`（:1402） | 完全没有 `collectUnknown` ⇒ 约束对象自身的未知字段无归属（只写不读） |

第 3 条目前被第 1 条"掩盖"（约束整段被当骨架未知字段回写，未知字段**碰巧**被保留）；
**修完第 1 条后它会立刻暴露**，必须与 1、2 同批修。

### 3.3 为什么 116 / 70 全绿却没发现

- 现有 format 测试最多只做 `decode → encode → decode` **一轮**，而 v2 输入路径下**第一次保存不重复**（重复出现在第二次保存后）；
- 没有任何断言检查「encode 输出无重复键」；
- B1/B2 的**新增断言为 0**（计划 §5 要求 core ≥15 / format ≥15）⇒ 新字段与新对象**从未被往返测试覆盖**。

⇒ 这不是"测试运气不好"，而是**缺断言直接掩盖了致命缺陷**。

### 3.4 返修建议

1. `kSkeletonKeys` 追加 `ikConstraints` / `transformConstraints`。
2. `kAttachmentKeys` **按 `type` 分派**（region 一套、mesh / weightedMesh / linkedMesh 一套）——
   用并集会让「region 附件里写了 `uvs`」被静默当成未知字段回写而不报错，按 type 分派可顺带钉住。
3. 两个约束对象各加 `collectUnknown(...)` + 自己的已知键集合（`OwnerKind::kIkConstraint` / `kTransformConstraint`）。
4. **必须新增回归断言**：`decode → encode → decode → encode → decode` 多轮循环不产生重复键
   （这正是 §4 缺失的新增断言中优先级最高的一条）。
5. 修完后 B5 语料闭环**必须重跑**（落盘 v3 → 重开是 smoke 的主路径）。

---

## 4. 其余问题

| # | 严重度 | 问题 |
|---|---|---|
| **G1** | 高 | **B1 / B2 新增断言为 0**：计划 §5 要求 core ≥15、format ≥15（类型字段、校验拒绝、v3 往返、迁移纯函数、未知字段 round-trip、锚点）。报告 §6 已诚实披露，但它是验收判据的一部分 ⇒ 未满足 |
| **G2** | 中 | 约束的未知字段**只写不读**（§3.2 第 3 条）。修 P0 时一并处理 |
| **G3** | 低 | `kFormatTooNew` / `kCapabilityUnsupported` 命名分歧（v3 提案 §6.1 vs v2 正文）未定 —— 报告 §6 已列出，不阻塞代码 |
| ~~G4~~ | 低 | ✅ TBD-E 已关闭（2026-10-22）：差 4 根因＝4 条附件名尾随空格致 `path ?? name` 未命中；三处数字各自口径一致，不影响建模/实现 |
| **G5** | 低 | `docs/format/native-project-v3-mesh.md` 未随实现同步（提案现状 ≠ 实现现状），A4 收尾项 |
| **G6** | 提示 | `kMeshUvOutOfRange` 判据与契约一致（RFC §11「拒绝」+ TBD-B 语料 0 例）。B5 若出现**大量**该诊断，须回头核对 A1 的 UV 统计，不得默认判据错了 |

---

## 5. 纪律核查

| 项 | 结果 |
|---|---|
| 未越界开工 B3 | ✅ `mapper.cpp` 的 20 行改动是 `Attachment` variant 化的**机械适配**（region 字段逐一照搬），无第二批解析逻辑 |
| 未擅自改契约 | ✅ 9 个诊断码、`IdKind` ×2、`OwnerKind` ×2 均**末尾追加**；region 语义未动 |
| 诚实披露既有断言改动 | ✅ 报告 §5 逐条列出 5 条；核对后确属 v3 的必然后果（`mesh` 已合法 ⇒ 改用 `ghost` 钉住未知类型；Reader 上限变 3 ⇒ 用 `formatVersion=4` 钉住越界） |
| 停点纪律 | ✅ B3 / B4 / B5 未开工；报告停在等复核 |
| 未提交 | ⚠️ B1/B2 改动**全部未提交**（工作区 17 个脏文件），`Type_Coverage_Status.md` 亦未提交 |

---

## 6. 结论与停点

**首轮不通过**（2026-10-22）；**二次复核已通过**（2026-10-22 晚，见 §7）。B1 / B2 的**代码质量是好的**（类型设计、判据落地、迁移纯函数、只增不改、诚实披露都到位），
但 **P0 使 v3 不可往返** —— 一个能让工程在第二次保存后打不开的实现，无论断言多绿都不能算完成。

返修清单（按优先级）：

1. **P0**：`decode.cpp` 三处已知键/未知字段收集补全（§3.2）→ 重跑多轮往返实测；
2. **G1**：补 B1/B2 新增断言（core ≥15 / format ≥15），**其中「多轮保存-重开无重复键」为必测**；
3. **G2**：随 P0 一并修；
4. G3–G5 为低优先级收尾项。

**停点（首轮）**：本对话只做验收与计划，不实现。返修由实现会话执行；完成后我再复核。
**二次复核已通过**：P0/G1/G2 均验证满足，B1/B2 验收解除，**解锁 B3**（B3 自身返修另计，详见 `Agent_Task/Task5_B3_Review.md`）；
通过后**不自动授权 B4/B5** —— 阶段 B 按「每完成一个工作包即停止等待复核」推进（计划 §9）。

---

## 7. 二次复核（2026-10-22 晚，B1/B2 返修后）

- 复核对象：工作区未提交的 B1/B2 返修改动（`decode.cpp` +327/−21；约束/附件已知键与未知字段收集补全；core +19、format +16 断言）
- 方法：干净构建目录 `build_b1b2_rereview` **全量重编 + 全量 ctest**，独立取各测试目标计数；重点重跑 P0 多轮往返用例
- 工具链：`PATH` 含 `.devtools/cmake` + `.devtools/mingw64`（MinGW-w64）；Defender 实时防护一度拦截 `g++.exe`，负责人放行后构建通过

### 7.1 复现结果

| 项 | 数值 | 结论 |
|---|---|---|
| 干净构建 `BUILD_EXIT` | **0** | ✅ |
| 编译 warning | **0** | ✅ |
| `ctest` | **7/7 通过** | ✅ |
| `format_tests` | **86 passed, 0 failed**（基线 70 → **+16**，≥15 下限 ✅） | ✅ |
| `core_tests` | **135 passed, 0 failed**（基线 116 → **+19**，≥15 下限 ✅） | ✅ |
| `binary_read_tests` | 27/27 | ✅ |
| `spine32_mapper_tests` | 32/32 | ✅ |
| `atlas_import_tests` | 8/8 | ✅ |
| `skeleton_import_tests` | 27/27 | ✅ |

### 7.2 P0 修复验证（关键）

`tests/format/test_v2_skeleton.cpp:929` `multi_roundtrip_has_no_duplicate_keys` 实际执行
`decode → encode → decode → encode → decode → encode` 三轮往返，并断言 `e1 == e3`（多轮稳定）。
本轮实测 **PASS**；同文件 `duplicate_key_rejected` / `duplicate_key_inside_object_rejected` 亦 PASS。
首轮 §3 的「v2 风格第 2 次保存即重复键、mesh 第 1 次保存即重复键」两类失败路径，**现已各有回归断言覆盖**。

### 7.3 结论

**二次复核通过。** B1/B2 返修满足 P0（v3 多轮往返无重复键）、G1（core ≥15 / format ≥15 新增断言）、
G2（约束未知字段读+写）全部判据；干净构建零 warning、ctest 零回归。B1/B2 验收解除，**解锁 B3**。
B3 自身首轮复核不通过（`Task5_B3_Review.md`，fixture golden 未做 + 断言下限未达），返修完成并复核通过前不得开工 B4/B5。
**不自动授权 B4/B5。**
