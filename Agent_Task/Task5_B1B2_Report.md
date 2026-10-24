# Task 5 阶段 B：B1 + B2 完成报告（2026-10-22）

- 上游：`Agent_Task/Task5_Implementation_Plan.md`（工作包定义）、`Agent_Task/Task5_A6_Acceptance.md`（阶段 A 验收通过）
- 状态：**B1 + B2 代码完成、全量回归已跑通**；评审 P0 / G1 / G2 的返修已落实（见 §3.5 / §3.6）；**B3 / B4 / B5 未开工**；复核结论见 `Task5_B1B2_Review.md`（首轮不通过 → 返修后待二次复核）
- 停点：按计划 §9「每完成一个工作包即停止等待复核」，此处停下

## 1. 为什么 B1 与 B2 合并

开工后发现二者**无法解耦**：`encodeDocument()` 返回 `std::string`、encode 内部无 `fail()` 通道
（`format/include/loongbones/format/codec.hpp:81`）。若 B1 只改 Domain，第二批数据在 B2 之前
**只能被静默丢弃**（encode 无路可报错），中间态可被误读为「已支持持久化」。
故按「一起完成」的指示合并推进，杜绝中间态。

## 2. B1 —— Domain 第二批类型与校验

| 文件 | 改动 |
|---|---|
| `domain/include/.../ids.hpp` | 追加 `IdKind::IkConstraint` / `TransformConstraint`（末尾追加，既有取值不变） |
| `domain/include/.../diagnostics.hpp` | 追加 9 个码（只追加）：`MESH_TRIANGLE_INDEX_OUT_OF_RANGE` / `MESH_VERTEX_GEOMETRY_INVALID` / `MESH_UV_OUT_OF_RANGE` / `MESH_LINKED_PARENT_UNRESOLVED` / `MESH_WEIGHT_SUM_INVALID` / `MESH_WEIGHT_BONE_UNRESOLVED` / `MESH_BINDING_COUNT_INVALID` / `CONSTRAINT_TARGET_UNRESOLVED` / `CONSTRAINT_CHAIN_EMPTY`；非有限浮点**复用** `SKELETON_VALUE_NOT_FINITE` |
| `domain/src/diagnostics.cpp` | 9 个码的稳定字符串 |
| `domain/include/.../skeleton.hpp` | 新增 `MeshAttachment` / `VertexBinding` / `WeightedMeshAttachment` / `LinkedMeshAttachment` / `Attachment`（`std::variant`）/ `BoundingBoxAttachment`（**空占位，不进 variant**，D32）/ `IkConstraint`（含 `enabled`，D35）/ `TransformConstraint`（10 个 float 原值，D36）；`SkinEntry::attachment` 改为 `Attachment`；`Skeleton` 加 `ikConstraints` / `transformConstraints`；4 个附件类型与 `VertexBinding` 各加 `operator==` |
| `domain/src/skeleton.cpp` | 附件校验按 variant 分派；新增几何 / UV / 三角 / 绑定 / 权重和 / 父引用闭合；新增 `checkConstraints()` |

判据落地：
- **D34**：单条负权重合法，校验只看**权重和**；**D37**：和必须**有限且 > 0**（0 / 负 / NaN 均 `MESH_WEIGHT_SUM_INVALID`）。
- **D38**：linked 父引用**先全量收集 `(slotId, attachmentName)` 再校验**（两阶段），避免重演旧 Reader「找不到父」。
- **D31**：`hull` / `edges` 不进类型（不落 canonical）。
- **D36**：约束的 mix / offset **不解释语义**，只校验有限性与引用闭合。

## 3. B2 —— 原生格式 v3（三读一写）

| 文件 | 改动 |
|---|---|
| `format/include/.../codec.hpp` | `kFormatVersion` 2→**3**、`kReaderMaxVersion` 2→**3**（`kReaderMinVersion` 保持 1）；新增 `kLegacyV2Version`；声明 `migrateV2ToV3()` |
| `format/include/.../project_document.hpp` | `OwnerKind` 追加 `kIkConstraint` / `kTransformConstraint`；`SourceFormatVersion` 追加 `kV3` |
| `format/src/encode.cpp` | 骨架写出 `ikConstraints` / `transformConstraints`（**键恒存在**，可空数组）；附件按 `type` 分派 4 种（region 路径**逐字未改**）；新增 `emitMeshAttachment` / `emitWeightedMeshAttachment` / `emitLinkedMeshAttachment` / `emitIkConstraint` / `emitTransformConstraint` |
| `format/src/decode.cpp` | `migrateV2ToV3()`（**纯函数**：只标记版本，不生成 ID、不改字段、不碰磁盘、幂等）；迁移链 v1→v2→v3 在 `decodeDocument` 内串联；附件按 `type` 严格分派 4 种，**未知类型显式拒绝**（不做「当作 region」降级）；骨架读取两个约束键（v2 缺失 ⇒ 空数组，等价迁移结果） |

JSON 形状按 `docs/format/native-project-v3-mesh.md`：linked 用 `parentKey: { slotId, attachmentName }`；
weighted 用 `vertexBindings[][]{ boneId, x, y, weight }`（顶点数 = 外层长度，**没有** `vertices`）。

## 3.5 评审意见的落实（2026-10-22 第二轮，P0 / G1 / G2）

| 评审项 | 落实内容 |
|---|---|
| **P0-1 已知键补全** | `kSkeletonKeys` 补 `ikConstraints` / `transformConstraints`（缺了会被收成未知字段并回写成**重复键**）；新增 `kIkConstraintKeys` / `kTransformConstraintKeys` / `kParentKeyKeys` / `kVertexBindingKeys` |
| **P0-2 附件键表按 type 分派** | 拆成 `kRegionAttachmentKeys` / `kMeshAttachmentKeys` / `kWeightedMeshAttachmentKeys` / `kLinkedMeshAttachmentKeys`；**不用并集** —— 并集会让 region 附件里的 `uvs` 被当成已知键而**静默丢弃**。分派后类型不符的字段走未知字段通道（保留并回写），不丢也不误解析 |
| **P0-3 多轮往返无重复键** | 新增 `v3.multi_roundtrip_has_no_duplicate_keys`：decode→encode→decode→encode→decode，**每轮 decode 都必须成功**（重复键会在下一轮被 `duplicate key` 拒绝），且首末两次 encode **逐字节相同** |
| **G1 新增断言** | core **+19**（B1 类型字段、权重和 > 0、负权重合法、三角/UV/绑定/引用/链空/ID 种类拒绝）；format **+16**（4 种附件往返、约束往返、迁移幂等、v3 被接受、v4 被拒、嵌套未知字段保留、多轮往返） |
| **G2 约束未知字段只写不读** | `decodeIkConstraint()` / `decodeTransformConstraint()` 补 `collectUnknown`（此前只写不读，锚点无来源）；另补齐嵌套位置：`parentKey` 内与 `vertexBindings[i][j]` 内的未知字段，**decode 与 encode 的子路径逐字一致**（`/attachment/parentKey`、`/attachment/vertexBindings/{i}/{j}`） |

另外修正了实现与提案的一处偏差并写回文档：权重含 NaN/±Inf 时**只**报 `SKELETON_VALUE_NOT_FINITE`，
不叠加 `MESH_WEIGHT_SUM_INVALID`（权重和此时无法判定，避免同一处重复报两条）。

## 3.6 复核返修（2026-10-22 第二轮，P0 / G1 / G2）

复核结论见 `Agent_Task/Task5_B1B2_Review.md`：**不通过** —— 存在阻断性缺陷 P0
（`decode.cpp` 三处已知键未同步 ⇒ 保存产生重复键 ⇒ 工程第二次保存后打不开，`kJsonDuplicateKey`）。

返修内容（逐条对应复核 §3.2 / §3.4 / §4）：

| 复核条目 | 返修 |
|---|---|
| P0-1 `kSkeletonKeys` 缺两个约束键 | 已补 `ikConstraints` / `transformConstraints` |
| P0-2 附件键表未按 type 分派 | 拆成 4 套（region / mesh / weightedMesh / linkedMesh）+ `kParentKeyKeys` + `kVertexBindingKeys`；**不用并集** |
| P0-3 约束对象无 `collectUnknown`（只写不读） | `decodeIkConstraint()` / `decodeTransformConstraint()` 各补 `collectUnknown` + 自己的已知键表 |
| P0-4 缺「多轮往返无重复键」回归断言 | 新增 `v3.multi_roundtrip_has_no_duplicate_keys`（decode→encode→decode→encode→decode，每轮 decode 必须成功，首末两次 encode 逐字节相同） |
| P0-5 修完须重跑语料闭环 | 已重跑骨架导入 smoke：语料 1113 → prepare 成功 1111、完整提交（含**落盘 v3 → 重开**）**8 / 8**、region 未命中 0；退出码 **7** = 已知共享图集布局缺口（2 个单位，Task 6），**非本次引入** |
| G1 新增断言 | core **+19**、format **+16**（均 ≥15） |
| G2 约束未知字段只写不读 | 随 P0 修好；另补齐**嵌套**位置（`parentKey` 内、`vertexBindings[i][j]` 内），decode 与 encode 的子路径逐字一致 |
| G5 v3 提案未随实现同步 | 已在 `docs/format/native-project-v3-mesh.md` 增补 §11「实现同步记录」（含已知键分派表、迁移实际行为、读取上限 3、嵌套子路径约定、未决项） |
| G3 / G4 | **命名分歧已消解（非未完项）**：`format_diagnostics.hpp:34-36` 三码并存且语义分明（kFormatVersionUnsupported / kFormatTooNew / kCapabilityUnsupported），`docs/format/native-project-v3-mesh.md:206-207` 已写明按 v2 冻结稿统一，修订在基线 `31c87f6` 内 —— 实现与提案一致，无需裁定。**TBD-E**（47,705/47,709/47,711 差 4）仍未核对，列为真实未完项 |

另外修正了实现与提案的一处偏差并写回文档：权重含 NaN/±Inf 时**只**报 `SKELETON_VALUE_NOT_FINITE`，
不叠加 `MESH_WEIGHT_SUM_INVALID`（权重和此时无法判定，避免同一处重复报两条）。

## 4. 验证（干净构建目录，未复用历史缓存）

```
cmake -S D:\Works\Spine\Loongbones -B D:\Works\Spine\.builds\build_b1b2 -G "MinGW Makefiles"
mingw32-make -C D:\Works\Spine\.builds\build_b1b2 -j8
ctest --output-on-failure      # 在 build_b1b2 下
```

| 项 | 结果 |
|---|---|
| cmake 配置 | exit **0** |
| 编译（MinGW Makefiles，`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`） | exit **0**，**零 error、零 warning** |
| ctest | **7 / 7 通过**（100%） |
| `loongbones_core_tests` | **135 / 135**（基线 116 → **+19** 条新断言，G1 要求 ≥15 ✅） |
| `loongbones_format_tests` | **86 / 86**（基线 70 → **+16** 条新断言，G1 要求 ≥15 ✅） |
| `loongbones_binary_read_tests` | **27 / 27**（基线 27，零回归） |
| 骨架导入 smoke（语料闭环：**落盘 v3 → 重开**） | 语料 1113 → prepare 成功 1111、完整提交 **8 / 8**、region 未命中 0；退出码 **7** = 已知共享图集布局缺口（2 个单位，Task 6），**非本次引入** |
| `binary_read_tests` / `spine32_mapper_tests` / `skeleton_import_tests` / `atlas_import_tests` / `format_probe_selftest` | exit **0** |

## 5. 既有断言 / 测试辅助的改动（**逐条列出，诚实披露**）

v3 升版使一批写死 v2 的断言**必然失效**；`Attachment` 改为 `std::variant` 也强制改了一批访问点。
**全部改动都在测试侧（断言 / 测试辅助函数 / 访问点），没有改动产品语义。**

### 5.1 版本字面量断言（6 处）

| # | 位置（TEST） | 改动前 | 改动后 |
|---|---|---|---|
| 1 | `test_v2_skeleton.cpp` · `v1_save_writes_v2_and_reopens_equivalent`（落盘） | 断言落盘含 `"formatVersion": 2` / `"minimumReaderVersion": 2` | 改为 **3 / 3** |
| 2 | 同上（重开） | `reopened.document.sourceVersion == kV2` | `== kV3` |
| 3 | 同上 · `v3_document_rejected` | v3 文档必须被拒绝 | v3 已被接受 ⇒ 改为 **`formatVersion=4`（超出 Reader 上限 3）必须被拒绝** |
| 4 | 同上 · `attachment_type_other_than_region_rejected` | `type=mesh` 必须被拒绝 | mesh 已合法 ⇒ 改为**未知类型 `ghost` 必须被拒绝** |
| 5 | 同上 · `encoder_always_writes_v2_even_for_v2_input` | 断言重编码输出含 `"formatVersion": 2` / `"minimumReaderVersion": 2` | 改为 **3 / 3**（**原报告漏列此项**） |
| 6 | `tools/format-tests/src/main.cpp` · `format_version_above_reader_rejected` | 用 `formatVersion: 3` 验证拒绝 | 改为 **4** |

> 注：#5 的 TEST 名仍叫 `encoder_always_writes_v2_even_for_v2_input`，但断言已改为期望 v3；
> 名称与内容不一致，建议改名（如 `encoder_always_writes_v3`），列为后续小项。

### 5.2 `Attachment` variant 化带来的机械改动（非版本断言，但同样只动测试）

- **`std::get` 访问点约 20 处**（core 测试 6 / format 测试 9 / mapper 测试 5）：
  `SkinEntry::attachment` 由 `RegionAttachment` 结构改为 `std::variant<...>` 后，
  原 `x.attachment.field` 直访全部改为 `std::get<RegionAttachment>(x.attachment).field`。
  纯机械，不改被测语义。
- **`sameAttachment` 签名改动**（`test_skeleton.cpp`）：
  由 `bool sameAttachment(const RegionAttachment&, const RegionAttachment&)` 改为
  `bool sameAttachment(const Attachment&, const Attachment&)`，并新增
  `return a == b;` 兜底分支（匹配非 region 变体）。**该兜底分支目前零覆盖** ——
  core 测试没有任何用例构造非 region 附件，故 `Attachment` 的 `operator==` 非 region 分支未被触发。
  建议补 1 条非 region 附件相等性用例以消除盲区。

## 6. 未完项（不粉饰）

| 项 | 说明 |
|---|---|
| ~~B1 / B2 的新增断言~~ | **已补齐**：core +19、format +16（G1 达成，见 §3.5）；多轮往返无重复键的回归断言已加 |
| **B3 / B4 / B5** | 未开工（Spine Reader/Mapper 扩展、插件诊断、语料闭环） |
| ~~`kFormatTooNew` / `kCapabilityUnsupported` 命名分歧~~ | **已消解（非未完项）**：三码语义分明且提案 §6.1 已按 v2 冻结稿统一（见 §3.6 G3/G4），实现与提案一致 |
| ~~TBD-E~~ | ✅ **已关闭（2026-10-22）**：差 4 根因＝查找键 `path ?? name` 对 4 条尾随空格附件名未命中；三处数字各自口径一致，不影响建模/实现（详见 `docs/mapping-spine32-to-native.md` U4） |
| **`encoder_always_writes_v2_even_for_v2_input` 测试名** | 断言已改期望 v3，但 TEST 名仍含 `v2`，名称与内容不一致，建议改名（见 §5.1 注） |
| **`sameAttachment` 兜底分支零覆盖** | `Attachment` 的 `operator==` 非 region 分支（core 测试无构造非 region 附件的用例），建议补 1 条相等性用例 |

## 7. 停点

产品代码、评审 P0/G1/G2 的补正、全量回归都已就绪；**B3–B5 未做**。请指示下一步：
（a）进入 B3（Spine Reader/Mapper 扩展，工作量最大：6 类附件落地 + 两阶段解析 + D38 两条 linked 路径）；
（b）先裁定 §6 的命名分歧（TBD-E 已关闭），再进 B3；
（c）先补 v3 fixture 与 B3 的 golden 表（`docs/skeleton/fixtures/README.md`）。
