# Task 4 阶段 B · B3 验收报告

- 日期：2026-10-21
- 范围：`Agent_Task/Task4_Implementation_Plan.md` B3（Native Format v2 双版本 Codec 与迁移）
- 结论：**B3 六项全部完成**。测试：core 116 / format 70 / atlas-import 8 全绿。
- 前置：B0（契约冻结，含 `sourceRef` 可选补项）已复核通过；B2（骨架 Domain 模型与校验）已实现。

---

## 1. B3 六项逐条对照

| # | B3 要求 | 完成情况 | 落点 |
|---|---|---|---|
| 1 | `ProjectDocument` 增加文档版本状态；增加 Skeleton/Bone/Slot/Skin/Entry/Attachment 对应的未知字段 `OwnerKind` | ✅ `SourceFormatVersion{kUnknown,kV1,kV2}` + `ProjectDocument::sourceVersion`；`OwnerKind` 追加 `kSkeleton`/`kBone`/`kSlot`/`kSkin`/`kSkinEntry`（只追加，不改既有取值）。**Attachment 故意不给独立 kind**：与 SkinEntry 1:1 内嵌，锚点 `(kSkinEntry, entryId)+/attachment` | `project_document.hpp` |
| 2 | Reader 同时接受 v1 与 v2：先判 `formatVersion` → `minimumReaderVersion` → required capabilities；v1 行为与 D24 一致；不支持的 required capability 报 `CAPABILITY_UNSUPPORTED` | ✅ 三段式顺序在 `decodeRoot` 中显式分段并注释；能力白名单按**文档版本**分档（v1 只认基线能力，v2 追加骨架能力） | `decode.cpp` `decodeRoot` / `isAcceptedCapability` |
| 3 | v1 先完整解码/校验，再经**纯函数**迁移为内存 v2：只补 `skeletons: []`、设置 v2 版本/能力字段；不生成 ID、不改旧字段、不碰磁盘 | ✅ `migrateV1ToV2()` 幂等；在 Domain `adoptData()` 通过**之后**才调用；optional 能力与未知字段原样保留 | `codec.hpp` / `decode.cpp` |
| 4 | v2 Reader 严格解码 Skeleton 并走 Domain `adoptData()`；未知字段锚点用 kind + 稳定 ID，不用数组位置 | ✅ 骨架全字段严格解码（含枚举字符串、color 0..1 范围、`type` 仅 `region`）；`collectUnknown` 全部以稳定 ID 为宿主 | `decode.cpp` 骨架层 |
| 5 | 新写入统一输出 v2，带 `minimumReaderVersion: 2` 与 required `loongbones.skeleton.v1`；打开 v1 后只有用户显式保存才写 v2；保持临时文件/回读校验/原子替换 | ✅ 编码器无条件写 v2；`project.skeletons` 键恒存在（可为空）；能力声明写出时自动补齐且不重复。IO 层未改动 | `encode.cpp` |
| 6 | 嵌套未知字段在重排/重命名/删除宿主时跟随或丢弃语义符合 RFC | ✅ 测试覆盖 skin entries 重排、bone 重命名，未知字段均按 ID 归位 | `test_v2_skeleton.cpp` |

---

## 2. 关键实现决定

### 2.1 文档版本状态是「来源版本」，不是「写出版本」

`ProjectDocument::sourceVersion` 记录的是**读入时磁盘上那个文件的版本**。写出永远是 v2，
所以不需要 `targetVersion` 字段。这样 `sourceVersion == kV1` 就是唯一的"这次会话是迁移来的 v2、
尚未落盘"判据 —— UI/宿主可用它提示"保存将升级工程"。

### 2.2 v1 文档若含 `project.skeletons` → 显式拒绝

v1 的 project 已知键里没有 `skeletons`，该字段会被收进 unknowns；迁移后又作为真字段写出，
最终同一个 JSON 对象里出现**两个 `skeletons` 键**。故 v1 路径显式报
`kFormatVersionUnsupported`，不猜测也不静默丢弃（Task 4 §6 纪律）。

### 2.3 v2 必须声明骨架能力 —— 在解码骨架数据**之前**检查

`checkDeclaredCapabilities()` 位于 `decodeRoot` 内、`decodeProject` 之前。理由：能力不匹配的
文档不该先被解析理解，这与"读取前即表态"的意图一致。缺失报 `kFormatMissingField`
（必需的东西没写），而不是新增一个窄义错误码。

### 2.4 能力白名单按版本分档

v1 文档声明 `loongbones.skeleton.v1` → `kCapabilityUnsupported`（v1 时代不存在该能力）。
这让"版本"与"能力"两套机制不会互相打洞。

### 2.5 迁移只补不改

`migrateV1ToV2` 只在缺失时插入能力项，已有项不动；`data.skeletons` 清空（v1 本来就没有）；
`sourceVersion` 保持 `kV1`（改了就丢失"尚未落盘"信息）。幂等，可无条件调用。

### 2.6 sourceRef 的 `null` 与省略

解码：`null` → `kJsonTypeMismatch`（v2 schema §3.7）；省略 → 不产生 `sourceRef`。
编码：省略 → 根本不写这个键；`null` 从不写出。往返测试中显式断言输出里没有 `sourceRef` 字样。

---

## 3. 测试新增（23 个用例，`tests/format/test_v2_skeleton.cpp`）

| 组 | 用例 |
|---|---|
| v1 兼容 | `v1_document_still_readable`、`v1_document_without_capabilities_migrates`、`v1_document_rejects_skeletons_field` |
| 不保存不变 | `v1_open_without_save_leaves_file_byte_identical` |
| 迁移往返 | `v1_save_writes_v2_and_reopens_equivalent`、`migration_is_idempotent` |
| 版本/能力错误码 | `v2_missing_skeleton_capability_rejected`、`v2_without_capabilities_block_at_all_rejected`、`v1_rejects_skeleton_capability`、`v2_unknown_required_capability_rejected`、`v2_missing_project_skeletons_rejected`、`v3_document_rejected` |
| 骨架严格解码 | `skeleton_roundtrip_with_region`、`source_ref_null_rejected`、`source_ref_roundtrip`、`attachment_type_other_than_region_rejected`、`color_out_of_range_rejected`、`skeleton_missing_bone_order_rejected`、`domain_rejects_bad_skeleton_reference` |
| 未知字段锚点 | `unknown_fields_follow_reordered_skin_entries`、`unknown_fields_follow_reordered_bones` |
| 写入恒定 | `encoder_always_writes_v2_even_for_v2_input`、`original_capabilities_preserved_not_rewritten` |

既有 `format_version_above_reader_rejected` 的补丁值由 2 改为 **3** —— v2 现在是被接受的格式，
"高于本 Reader"的取值随之上移。这是 B3 的预期行为变更，不是回归。

---

## 4. 过程中定位并修掉的一个真实风险

首轮运行时出现堆损坏（0xC0000374）。根因是给 `ProjectDocument` 增加成员后，
`tools/format-tests/src/main.cpp`、`importers/spine32/src/atlas_import.cpp`、
`domain/src/project.cpp` 等 TU 仍链接着按**旧布局**编译的对象 —— ODR 违例导致越界写。
修法是全量重编所有依赖 TU（新目录 `build_b3`），而非增量。

> 结构性提醒：`ProjectDocument` / `ProjectData` 的布局一旦变更，必须全量重编。
> 这也是方案 §4「不可并行改 `ProjectData`、`ProjectDocument`」的实际代价。

---

## 5. 交付物

| 文件 | 变化 |
|---|---|
| `format/include/loongbones/format/project_document.hpp` | 追加 5 个骨架 `OwnerKind`；新增 `SourceFormatVersion` 与 `ProjectDocument::sourceVersion` |
| `format/include/loongbones/format/codec.hpp` | `kFormatVersion=2`、`kReaderMaxVersion=2`、`kReaderMinVersion`、`kLegacyV1Version`、`kSkeletonCapability`；声明 `migrateV1ToV2` |
| `format/src/decode.cpp` | 三段式版本/能力分派；v1/v2 已知键分档；骨架层解码（Skeleton/Bone/Slot/Skin/Entry/Attachment/SourceRef）；`migrateV1ToV2` |
| `format/src/encode.cpp` | 统一输出 v2；`project.skeletons` 恒写出；`capabilities` 恒写出并补齐必需项；骨架层编码 |
| `tools/format-tests/src/test_v2_skeleton.cpp` | **新建**（23 个 B3 用例） |
| `tools/format-tests/CMakeLists.txt` | 注册新测试文件 |
| `tools/format-tests/src/main.cpp` | `format_version_above_reader_rejected` 的过高值 2 → 3 |

未改动：已审阅的 v1 RFC 文档、`format/src/project_io.cpp`（原子保存流程原样保留）。
