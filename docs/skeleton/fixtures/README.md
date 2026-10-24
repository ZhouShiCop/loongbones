# 合成 fixture（Task 4 阶段 A + Task 5 A5 第二批）

**不含任何游戏资产。** 全部由 `tools/skeleton-probe/make-fixture.mjs` 按
Spine 3.2.01 二进制布局**独立写出**的字节（不调用旧项目写入器、不以任何游戏文件为模板），
图集为 libGDX `.atlas.txt`，PNG 为脚本生成的纯色图。可再分发。

生成/重建：

```bash
node tools/skeleton-probe/make-fixture.mjs --verify
```

`--verify` 会回读自校验（用旧项目 Reader 作对照解析，非产品读取器）。

---

## 1. 清单

| fixture | 文件 | 覆盖点 |
|---|---|---|
| **01-minimal** | `.skel.bytes` + `.atlas.txt` + `.png` | 最小骨架：1 根骨骼 / 1 个插槽 / 1 个 `default` 皮肤 / 1 个 region 附件 / 0 动画 / 0 约束 |
| **02-nested-slots-regions** | `.skel.bytes` + `.atlas.txt` + `.png` | 嵌套骨骼（root→hip→chest→head）、3 个插槽、3 个 region 附件；含 **rotation=90**、**附件名 ≠ region 名（靠 `path` 指过去）**、图集 **`rotate: true`** |
| **03-bad-parent-index** | `.skel.bytes` | 畸形：第 3 根骨骼的 `parentIndex = 9`（只有 3 根骨骼）→ **必须被拒绝** |
| **04-truncated** | `.skel.bytes` | 畸形：文件被砍掉最后 8 字节 → **EOF/截断必须失败** |
| **05-non-finite-float** | `.skel.bytes` | 畸形：第 2 根骨骼的 `rotation` 是 **NaN**（字节 `0x7F 0xC0 …`，位于偏移 83）→ **必须被拒绝**（`SKELETON_FLOAT_NON_FINITE`，见 `../skeleton-domain-rfc.md` §6.11） |
| **06-minimal-mesh** | `.skel.bytes` + `.atlas.txt` + `.png` | **第二批**：1 个非加权 `MeshAttachment`（3 顶点 / 1 三角形 / uv 全在 `0..1`）；含 `hull=3` 与 `edges`（**不落 canonical**，D31）与 `width/height`。

> **2026-10-22 复核更正**：`edges` / `width` / `height` 只在 `nonessential = 1` 时出现，
> 而语料 **nonessential=1 只有 4 / 1113 单位（0.36%）**，`edges` 出现率 **67 / 20,250（0.33%）**
> （A1 §4.3）。⇒ 本 fixture 是**语料里的极少数形态**；占 99%+ 的 `nonessential = 0`
> （无 `edges` / `width` / `height`）形态**已由 `16-minimal-mesh-plain` 覆盖**（2026-10-22 补），
> 见 `../../Agent_Task/Task5_A5_Review.md` P1 / §8.1 |
| **07-weighted-mesh** | `.skel.bytes` + `.atlas.txt` + `.png` | **第二批**：加权 mesh，3 顶点 ×（1~2 绑定）；顶点 0 含 **负权重 `-0.2`** 与 **>1 的权重 `1.2`**（和为 1.0）→ **必须被接受**（D34） |
| **08-linked-mesh** | `.skel.bytes` + `.atlas.txt` + `.png` | **第二批**：父**非加权** mesh `body_mesh` + 2 个 `LinkedMeshAttachment`（`inheritDeform` **true / false** 各一）；子附件**没有**顶点区。类型字节 **4**、显式 `parentSkin='default'` —— **语料 0 例**（TBD-F），纯合成路径 |
| **09-mesh-bad-triangle** | `.skel.bytes` | 畸形：3 顶点但三角索引里出现 `9` → **必须被拒绝**（`MESH_TRIANGLE_INDEX_OUT_OF_RANGE`） |
| **10-mesh-bad-bone-index** | `.skel.bytes` | 畸形：加权 mesh 的 `boneIndex = 7`（骨架只有 2 根骨骼）→ **必须被拒绝**（`MESH_WEIGHT_BONE_UNRESOLVED`） |
| **11-constraints** | `.skel.bytes` | **第二批**：1 条 IK（`mix = 1` → `enabled = true`，D35）+ 1 条 transform（`rotateMix = 90`，10 个 float **原值透传、不解释语义**，D36）；无附件、无图集 |
| **12-mesh-bad-weight-sum-zero** | `.skel.bytes` | **畸形**：顶点 0 的两条绑定权重都是 `0` ⇒ **权重和 = 0** → **必须被拒绝**（`MESH_WEIGHT_SUM_INVALID`，D37）。顶点 1 / 2 正常（和 = 1） |
| **13-mesh-bad-weight-sum-negative** | `.skel.bytes` | **畸形**：顶点 0 的两条绑定是 `-0.5` / `-0.2` ⇒ **权重和 = -0.7 < 0** → **必须被拒绝**（`MESH_WEIGHT_SUM_INVALID`，D37）。判据是**和**不是单条符号：单条负权重只要和仍 > 0 就合法（D34，见 07） |
| **14-linked-mesh-implicit-skin** | `.skel.bytes` + `.atlas.txt` + `.png` | **第二批**：父**加权** mesh `body_wmesh` + 2 个子附件；类型字节 **5**、`parentSkin = null`（父在同皮肤内）—— **与语料 2 例逐字段对齐**；`inheritDeform` true / false 各一 |
| **15-ik-mix-zero** | `.skel.bytes` | **第二批**：1 条 IK，`mix = 0` ⇒ **`enabled = false`**（D35 的 false 分支；语料 302 例为此值，此前无任何样本）；`bendPositive = false`；无附件、无图集 |
| **16-minimal-mesh-plain** | `.skel.bytes` + `.atlas.txt` + `.png` | **第二批**：`nonessential = 0` 的最小 mesh —— **无 `edges` / `width` / `height`、骨骼末尾无 4 字节颜色**。这是语料 **1109 / 1113 单位**的形态（06 是仅 4 个单位的少数形态） |
| **17-shared-atlas** | **目录** `pair/hub/`：2 个 `.skel.bytes` + 1 个 `.atlas.txt` + 1 个 `.png` | **Task 6 A4 正向**：两个骨架共用一份 `<上级目录名>_<目录名>.atlas.txt`（`pair_hub.atlas.txt`）+ 单页 PNG。**与语料唯一一处 `h0164s5/watch/` 逐形态对齐**（2 个单位 / 1 图集 / 1 页）。图集内 2 个 region 分属两个骨架 ⇒ 不能靠"随便取一个"通过 |
| **18-ambiguous-atlas** | **目录** `amb/duo/`：1 个 `.skel.bytes` + **2 个** `.atlas.txt` + 2 个 `.png` | **Task 6 A4 负向（Q6-2）**：目录内 2 个候选图集且**都不与骨架同名**（`duo_alpha` / `duo_beta` vs `duo_unit`）⇒ **必须显式拒绝 + 可定位诊断**（列出候选数）。两个图集**都含** `duo_region` ⇒ "取第一个"也能解析成功，因此**只有显式拒绝才算对** |
| **19-multi-atlas-same-name** | **目录** `multi/pick/`：1 个 `.skel.bytes` + 2 个 `.atlas.txt` + 2 个 `.png` | **Task 6 A4 守卫（回归）**：目录内 2 个图集，但**骨架同名的一个存在**（`pick_unit.atlas.txt` + `decoy.atlas.txt`）⇒ 必须走**同名优先**，**不得**判歧义、**不得**取 decoy。对应语料 **5 个目录 / 11 个骨架**的真实情形 |

> **08 与 14 是两条不同的解析路径，缺一不可**：08 = 类型 4 + 显式 `parentSkin`（语料 **0 例**），
> 14 = 类型 5 + `parentSkin = null`（语料 **2 例**，唯一真实形态）。负责人裁定（2026-10-22）：
> 「最好对齐，不过不对齐也没问题，**解析器需要对齐**；我们用的（原生）格式没关系」
> ⇒ 解析器两侧都要能处理，canonical 侧不受影响（D22 的 `(kind, 稳定 ID)` 锚点与皮肤名无关）。

**与模型提案 `mesh-and-constraint-rfc.md` §12 的命名对应关系**（避免阶段 B 对不上号）：

| RFC §12 逻辑名 | 本目录实际文件 |
|---|---|
| `v3-mesh-min` | `06-minimal-mesh`（`nonessential=1`）+ **`16-minimal-mesh-plain`**（`nonessential=0`，语料主流形态） |
| `v3-mesh-weighted` | `07-weighted-mesh` |
| `v3-mesh-linked` | `08-linked-mesh` + **`14-linked-mesh-implicit-skin`**（类型 4 / 类型 5 各一） |
| `v3-mesh-invalid` | `09-mesh-bad-triangle` + `10-mesh-bad-bone-index` + **`12-mesh-bad-weight-sum-zero`** + **`13-mesh-bad-weight-sum-negative`** |
| `v3-constraints` | `11-constraints` + **`15-ik-mix-zero`** |

---

## 2. 预期结果（阶段 B 的断言依据）

| fixture | 期望 |
|---|---|
| 01-minimal | 解析成功；`bones=1 slots=1 skins=1 attachments=1 animations=0`；region `region-0` 解析到 `01-minimal.atlas.txt` 的同名 region；无 error 级诊断 |
| 02-nested-slots-regions | 解析成功；`bones=4 slots=3 skins=1 attachments=3`；`head-skin-a` 的 `path=head` → 解析到 region `head`；`weapon` 的 `rotationDeg=90`、`x=4`、`y=-2`；图集 region `head` 的 `rotation == "90"`（源 `rotate: true`） |
| 03-bad-parent-index | **拒绝**，诊断 `SKELETON_BONE_PARENT_OUT_OF_RANGE`，location 指向该骨骼；候选整体丢弃，工程不变 |
| 04-truncated | **拒绝**，截断/EOF 诊断（带字节偏移）；候选整体丢弃 |
| 05-non-finite-float | **拒绝**，诊断 `SKELETON_FLOAT_NON_FINITE`，`sourceLocation` 含段名（`bones`）与**字节偏移**；候选整体丢弃 |
| 06-minimal-mesh | 解析成功；`bones=1 slots=1 skins=1 attachments=1`；附件类型 `MeshAttachment`；`uvs` 6 个浮点、`triangles=[0,1,2]`、`vertices=[0,0,32,0,16,32]`；`hull=3` 与 `edges=[0,1,1,2,2,0]` **不落 canonical**（D31，计入 `report.unsupported`）；`width=height=32` |
| 07-weighted-mesh | 解析成功；3 顶点；顶点 0 的绑定为 `[(bone-a, 0, 0, 1.2), (root, 10, 0, -0.2)]` —— **负权重合法**（D34），权重和 = 1.0；**不拒绝、不诊断、不归一化、不舍入** |
| 08-linked-mesh | 解析成功；同一 slot 下 3 个附件：`body_mesh`（mesh，类型 4 的父）+ `body_mesh_linked`（`inheritDeform=true`）+ `body_mesh_linked_noffd`（`false`）；子附件用 `parentKey=(slot-body, body_mesh)` 引用父、**不复制** `uvs/triangles/vertices` |
| 09-mesh-bad-triangle | **拒绝**，诊断 `MESH_TRIANGLE_INDEX_OUT_OF_RANGE`（索引 `9` ≥ 顶点数 `3`）；候选整体丢弃 |
| 10-mesh-bad-bone-index | **拒绝**，诊断 `MESH_WEIGHT_BONE_UNRESOLVED`（`boneIndex=7`，骨架 2 骨）；候选整体丢弃 |
| 11-constraints | 解析成功；`ikConstraints=1`（`mix=1` ⇒ `enabled=true`，D35；`bendPositive=true`）、`transformConstraints=1`（`rotateMix=90` 等 10 个 float **原值透传**，D36）；无附件 |
| 12-mesh-bad-weight-sum-zero | **拒绝**，诊断 `MESH_WEIGHT_SUM_INVALID`（顶点 0 权重和 = **0**）；候选整体丢弃。**不做**归一化、不把 0 当成"均匀分摊" |
| 13-mesh-bad-weight-sum-negative | **拒绝**，诊断 `MESH_WEIGHT_SUM_INVALID`（顶点 0 权重和 = **-0.7**）；候选整体丢弃。**单条**负权重仍合法（见 07），只有**和 ≤ 0** 才拒绝 |
| 14-linked-mesh-implicit-skin | 解析成功；同一 slot 下 3 个附件：`body_wmesh`（weightedMesh，3 顶点 × 1 绑定，权重和均 = 1.0）+ `body_wmesh_linked`（`inheritDeform=true`）+ `body_wmesh_linked_noffd`（`false`）；两条子附件 `parentSkin = null` ⇒ 父引用在**同皮肤内**解析，`parentKey=(slot-body, body_wmesh)` |
| 15-ik-mix-zero | 解析成功；`ikConstraints=1`（`mix=0` ⇒ **`enabled=false`**，`bendPositive=false`；`mix` 原值 0 **保留**，D35）；无附件 |
| 16-minimal-mesh-plain | 解析成功；`bones=1 slots=1 skins=1 attachments=1`；`nonessential = 0` ⇒ **无** `edges` / `width` / `height` 可读（不得臆造），`hull=3` 同样不落 canonical；`uvs=[0,0,1,0,0.5,1]`、`triangles=[0,1,2]`、`vertices=[0,0,32,0,16,32]` |
| **17-shared-atlas** | 两个骨架**都**解析成功；`pair_hub_left` ⇒ `bones=2 slots=1 attachments=1`、region `hub_left`；`pair_hub_right` ⇒ 同计数、region `hub_right`；两者**都**解析到**同一份** `pair_hub.atlas.txt`（同一 `Atlas.id`，**不是**各建一份副本，D53）；工程内 `atlases=1` / `textures=1`；`draft.resources` 对同一 PNG **不重复**登记；重开后仍为 1 份（D40 / D53） |
| **18-ambiguous-atlas** | **拒绝**，诊断＝**歧义**（`ATLAS_CANDIDATE_AMBIGUOUS` 或阶段 B 定名的等价码），**必须**含候选数（2）与候选名单的可定位信息；候选整体丢弃、工程不变。**不得**：取 `duo_alpha` 或 `duo_beta` 中任一个、不得静默成功 |
| **19-multi-atlas-same-name** | 解析成功；走**同名优先** ⇒ 图集为 `pick_unit.atlas.txt`，region `pick_region` 命中；**不得**判歧义、**不得**取 `decoy.atlas.txt`（decoy 里没有 `pick_region`，误取会表现为 region 未命中，可定位）。这是语料 11 个"多图集目录内靠同名消解"单位的**回归守卫** |

**关于 03 / 05 / 09 / 10 / 12 / 13 的重要说明**：旧项目的宽松 Reader 能把这六个文件**读完**
（03 只对越界父索引记一条 warn；05 不校验浮点有限性；09 / 10 不校验三角索引与骨骼下标；
12 / 13 不校验权重和）—— 这正是 fixture 要钉住的语义差别：**产品读取器必须拒绝**，
不能沿用"能读完就算成功"的宽松口径。

生成器 `--verify` 的当前实测输出（**19 个** fixture；17 有两个骨架故两行）：

```
01-minimal:                     ok  bones=1 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
02-nested-slots-regions:        ok  bones=4 slots=3 skins=1 anims=0 version=3.2.01 warnings=0
03-bad-parent-index:            ok  bones=3 slots=1 skins=1 anims=0 version=3.2.01 warnings=1
                                   warn: bones.b — 找不到骨骼索引为 9 的父骨骼
04-truncated:                   FAIL — 读取 float 越界
05-non-finite-float:            ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
06-minimal-mesh:                ok  bones=1 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
07-weighted-mesh:               ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
08-linked-mesh:                 ok  bones=1 slots=1 skins=1 anims=0 version=3.2.01 warnings=2
                                   warn: skins.slot-body.body_mesh_linked — linkedmesh 找不到父附件 default/body_mesh
                                   warn: skins.slot-body.body_mesh_linked_noffd — 同上
09-mesh-bad-triangle:           ok  bones=1 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
10-mesh-bad-bone-index:         ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
11-constraints:                 ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
12-mesh-bad-weight-sum-zero:    ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
13-mesh-bad-weight-sum-negative:ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
14-linked-mesh-implicit-skin:   ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=2
                                   warn: skins.slot-body.body_wmesh_linked — linkedmesh 找不到父附件 null/body_wmesh
                                   warn: skins.slot-body.body_wmesh_linked_noffd — 同上
15-ik-mix-zero:                 ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
16-minimal-mesh-plain:          ok  bones=1 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
17-shared-atlas [pair_hub_left]:  ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
17-shared-atlas [pair_hub_right]: ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
18-ambiguous-atlas:             ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
19-multi-atlas-same-name:       ok  bones=2 slots=1 skins=1 anims=0 version=3.2.01 warnings=0
```

> 08 / 14 的 warn 文本在控制台里会因**旧参照 Reader 源码的编码问题**显示成乱码
> （同一条消息，ASCII 部分正常）；上面按语义**规范化**后记录，实际判断以 `warnings=2`
> 与 `default/...`、`null/...` 的皮肤前缀为准 —— 这两条前缀正是 08 与 14 的路径差异。

05 / 09 / 10 / 12 / 13 的 `ok` 是**旧宽松 Reader 的行为**（它不校验这些），**不是**期望行为；
产品读取器对 05 / 09 / 10 / 12 / 13 必须拒绝（见上表）。

**08 与 14 的 warn 都是旧 Reader 的实现局限**，不是 fixture 错误：它解析 `linkedmesh` 时
`default` skin 尚未 push 进 `data.skins`（`readSkin` 返回后才 push），所以按名字找不到父附件
（`parentSkin = null` 时自然也找不到）。
产品 Reader **应当**能解析父引用（先收集 skin 再解析引用，或两阶段解析）——
属阶段 B 的验证项，fixture 只提供结构与字节，不声称产品行为已验证。

> **类型字节的独立验证**：`type-byte-census.mjs`（不经旧 Reader 的继承回退，直接读字节）
> 对本目录的普查结果 —— region **4**（01 ×1 + 02 ×3）、mesh **4**（06、16、08 的父 `body_mesh`、
> 09）、weightedMesh **5**（07、10、12、13、14 的父 `body_wmesh`）、类型 4 **2 条**（08）、
> 类型 5 **2 条**（14），父类型分别为 `mesh(2)` 与 `weightedMesh(3)`。与生成器意图逐条一致。
> （04 / 05 是故意的畸形样本，普查报 TRUNCATED / NON_FINITE_FLOAT 属预期。）

## 3. 不含什么

- 不含任何游戏资产、不含源二进制片段、不含真实图集/纹理
- PNG 为脚本生成的**纯色图**（01 红、02 蓝、06 黄、07 绿、08 紫、14 橙、16 青、17 蓝紫、18 红/绿各一、19 黄/紫各一），仅用于验证纹理链接与 IHDR 尺寸解析
- 不含绝对路径、盘符、用户名
- **合成 fixture 不等于兼容声明**（D13）：结构层覆盖 ≠ 语义已验证，见 §4 末的提醒

## 4. 与真实语料的关系

fixture 只覆盖**结构层**与**拒绝语义**。以下能力语料里**没有真实样本**，
因此只能由 fixture 覆盖结构、语义仍需待验证（见 `../skeleton-domain-rfc.md` §11.2）：

| 能力 | 语料情况 | fixture 覆盖程度 |
|---|---|---|
| **共享图集布局**（多骨架共用一份图集） | **1 处 / 2 个单位**（`h0164s5/watch/`）—— 唯一有真实样本的 Task 6 形态 | **已覆盖**：17（正向，逐形态对齐）、18（歧义拒绝，语料 0 例）、19（同名优先守卫，对应语料 5 目录 / 11 单位） |
| 多皮肤 | 0 个多皮肤单位（1113/1113 单皮肤） | 未覆盖（需后续补 fixture） |
| 多页图集 | 0 例多页 | 未覆盖（需后续补 fixture） |
| path 约束 / boundingbox / clipping | 0 例 | 未覆盖 |
| 图集 trim | 0 例（`size == orig` 100%） | 未覆盖 |
| 图集 `index`（sequence） | 0 例非 -1 | 未覆盖 |
| boundingbox 附件 | 0 例（D32 占位不实现） | 未覆盖（只有类型字节值，无样本） |
| UV 越出 `[0,1]` | 语料 **0 例** | 未覆盖（`MESH_UV_OUT_OF_RANGE` 的拒绝策略**无真实样本可验证**） |

> **D13 提醒**：本目录全部是**合成样本**，覆盖的是**结构层**与**拒绝语义**。
> 尤其 07（负权重）、08 / 14（父引用解析）、11 / 15（约束语义）、12 / 13（权重和拒绝）：
> fixture 提供字节与结构，**不声称**真实语料的语义已验证 ——
> 07 的负权重语义来自裁定 D34（"该绑定无影响"），12 / 13 的拒绝阈值来自 D37（"和 ≤ 0 无意义"），
> 08 / 14 的父引用解析**尚未被任何实现验证**（旧 Reader 只是找不到父，见 §2 末），
> 11 / 15 的 mix / offset 语义按 D35 / D36 **刻意不解释**，`enabled` 只是 `mix != 0` 的记录，
> 等 V3 时基与求值阶段取证。
>
> **真实样本支撑情况**：14 与语料 2 例**逐字段对齐**（类型 5 + `parentSkin = null`），
> 是唯一有真实样本的第二批 linked 形态；08（类型 4）与 12 / 13（非法权重和）语料 **0 例**，
> 属纯合成覆盖 —— 阶段 B 不得把它们当成"已验证的语料兼容"。
