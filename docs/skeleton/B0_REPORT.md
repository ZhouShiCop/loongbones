# Task 4 阶段 B · B0 验收报告

- 日期：2026-10-20（初版）；2026-10-20（**退回补项修订版**）
- 范围：`Agent_Task/Task4_Implementation_Plan.md` B0（Canonical 数据类型、v2 Schema 与导入资源策略冻结）
- 结论：初版 **暂不通过**（负责人退回一项关键契约）；补项已复核通过，**B0 验收通过**。
  本工作包**只改文档**，未写任何产品代码；B1–B6 尚未启动，且需分别获准后开始。
- 前置：阶段 A 已由负责人复核通过（`ACCEPTANCE.md` §10）。
- 复核结论原文要点：SourceRef 不得为每个 Skeleton 的必需字段（原生创建的数据无处可填 / 会诱使伪造来源）；
  `loongbones.skeleton.v1` 与 `SkinEntry.id` 两项按文档当前方案定案；并要求注明 DragonBones 对照范围。
- 复核结果：SourceRef 可选语义、能力范围、SkinEntry 稳定 ID 与 DragonBones 证据范围均符合要求；无阻塞项。B0 通过；B1 未获批准。

---

## 1. B0 九项逐条对照

| # | B0 要求 | 完成情况 | 落点 |
|---|---|---|---|
| 1 | 复核并冻结 `canonical-visual-data-types.md` 的类型边界；不得把「图片文件、图集区域、场景图片节点、骨架附件」合并成一个类型 | ✅ 类型分层（§1）与类型目录（§2）保留并冻结；§6 表第 4 项明确 `RegionAttachment` 与 Scene `SpriteNode` 是不同类型（可引用同一 `AtlasRegion`，但变换父级、持有者、保存位置不同） | `canonical-visual-data-types.md` §1/§2/§6 |
| 2 | 冻结 2D 模型的几何表达：Mesh 顶点、三角索引、regionUV 的归属；非加权与加权顶点语义分开；当前不实现 Mesh/WeightedMesh，只固定扩展边界 | ✅ §3.1/§3.2 冻结：UV = regionUV（`[0,1]`，不存整页像素坐标）；`triangles` 为顶点索引、长度须为 3 的倍数；非加权与加权**分开**（加权不展平为单骨骼索引、不写入 RegionAttachment）；扩展走独立字段 + required capability，旧 Reader 拒绝而非静默降级。§6.1 明确「逐字段数值映射」「权重归一化容差」仍未冻结 | `canonical-visual-data-types.md` §3/§6.1 |
| 3 | 对每个后续功能按 §7 建「功能目标 → 源证据 → 运行语义 → Loongbones 契约 → 类型分层 → 转换/降级 → 演进与测试」分析表；绑定/蒙皮作为案例 | ✅ 新增 **§7.0 首批四项功能对照表**（TextureAsset 存储、AtlasPage 页层缩放、RegionAttachment、SkinEntry 宿主与 ID），每张表七行齐全、均标注语料证据。§7.1 绑定/蒙皮案例保留为后续分析，并列出四个待定概念 | `canonical-visual-data-types.md` §7.0.1–§7.0.4 |
| 4 | 「最小基础、稳健扩展」：首期只实现已验证的必要类型；稳定 ID、引用/所有权、unknown-field 锚点、能力声明与版本演进规则必须预先闭合；不得预造空泛通用层，不得把源格式专属字段塞进 canonical 类型 | ✅ v2 schema **只序列化首批冻结类型**（§10 列出明确不包含的内容）；ID/锚点/能力/迁移/错误码全部成文；`SourceRef` 是唯一的源格式元数据容器，且两个 hash 字段已拆分 | `native-project-v2-skeleton.md` §1/§10；`canonical-visual-data-types.md` §7 设计取舍 |
| 5 | 新增 v2 schema 文档：完整 JSON 示例、字段必需性/默认值、枚举字符串、ID 类型、未知字段锚点、required capability、迁移和错误码；只序列化已冻结的首批类型 | ✅ 新建 `docs/format/native-project-v2-skeleton.md`：§2 完整 JSON 示例（1 工程/1 场景/1 图集/1 骨架/1 RegionAttachment，可独立实现）；§3 七张字段表（必需性 + 默认值 + 错误码）；§3.3/§3.6 枚举字符串（`blendMode`、`type`）；§4 ID 规则；§5 锚点；§6 能力与版本；§8 迁移；§9 诊断等级；§10 不包含清单。**补项修订：`sourceRef` 由「必需」改为「整个对象可选」（§3.7）** | `docs/format/native-project-v2-skeleton.md` |
| 6 | 明确 Skin/SkinEntry/Attachment 的 ID/未知字段宿主规则；若选无 ID 方案必须证明重排后仍正确归属 | ✅ **SkinEntry 增加 `id`（ULID）+ `OwnerKind::kSkinEntry`**；`Skeleton`/`Bone`/`Slot`/`Skin` 同样新增各自 `OwnerKind`；`Attachment` 不单独给 ID（1:1 内嵌，锚点 `(kSkinEntry, entryId) + /attachment`）。§5.3 记录三个候选方案与被否决理由（自然键可被编辑、无 ID 方案无法证明） | `native-project-v2-skeleton.md` §5；`canonical-visual-data-types.md` §6 表第 1 项 |
| 7 | 明确资源落地策略：图集页 PNG 导入工程内、存项目相对路径；源 PNG 只读经 `IResourceResolver`，由宿主资源暂存服务写入；插件无任意路径写权限 | ✅ `storage = "inProject"`、`location = assets/textures/<TextureAssetId>.png`（用 ID 而非 name，避免重命名影响路径）；读写拆成 `IResourceResolver`（只读）与 `IResourceStaging`（写盘）两个接口；Domain 校验通过后才写盘，失败回滚；`external` 保留给用户显式创建，导入器永不产出；Format 层补 `location` 形状校验规则 | `native-project-v2-skeleton.md` §7；`plugin-host-rfc.md` §4.1/§7.5 |
| 8 | 固定 source hash 语义：`sourceHeaderHash` 可选源字段；`contentHash` 必须由宿主对 `.skel.bytes` 全文计算 SHA-256；纹理摘要不进入工程 | ✅ v2 §3.7 字段表 + `skeleton-domain-rfc.md` §7.2/§8.3 + `field-mapping.md` §1 三处一致；纹理摘要明确不持久化，且与两个 hash 均非同义词。**补项修订：整个 `sourceRef` 对象可选**（§2.1） | `native-project-v2-skeleton.md` §3.7；`skeleton-domain-rfc.md` §8.3 |
| 9 | 更新 Task 4 阶段 B 验收范围：未进入切片的数据只产生统计与诊断，不创建半成品 Domain 对象；unsupported 为 warning，结构损坏/非法引用/缺纹理/非有限浮点为 error；warning 候选仅在用户明确确认后可提交 | ✅ v2 §9 诊断等级表 + `plugin-host-rfc.md` §9 验收项（含「error 拒绝整个候选；warning 仅在调用方显式 `acceptWarnings` 后可提交」） | `native-project-v2-skeleton.md` §9；`plugin-host-rfc.md` §9 |

---

## 2. 退回补项与修订结果（2026-10-20）

### 2.1 退回的关键契约：`sourceRef` 不得为必需

**负责人意见**：SourceRef 被设为每个 Skeleton 的必需字段。Loongbones 的原生数据也应能创建
没有 Spine/DragonBones 等来源的骨架；强制 SourceRef 会让原生创建的数据无处可填，
或诱使实现者伪造来源信息。建议将整个 `sourceRef` 对象改为可选。

**修订**：

| 处 | 改动 |
|---|---|
| v2 schema §3.1 | `sourceRef` 行：`必需` → **可选**，默认「省略（无来源元数据）」 |
| v2 schema §3.7 | 标题改为「`SourceRef`（整个对象**可选**）」；新增三情形表（省略 = 合法 / 存在 = 逐字段校验 / `null` = **不合法**） |
| v2 schema §2 | JSON 示例前加说明：示例骨架是导入产生的所以带 `sourceRef`；原生创建应整个省略该键 |
| v2 schema §5.2 | 锚点表后加注：可选容器的锚点规则只在容器**出现**时生效；原生创建（无 `sourceRef`）不影响锚点集合 |
| `skeleton-domain-rfc.md` §5.1 | `sourceRef` 类型改为 `SourceRef?`，注明「导入时提供，原生创建时省略」 |
| `skeleton-domain-rfc.md` §8.3 | 标题与正文改为「整个对象可选」+ 三情形表 + 修订理由 |
| `skeleton-domain-rfc.md` §10.1 | 实现范围措辞改为「导入时产出 `sourceRef`；原生创建时省略」 |

**`null` 与「省略」不等价**：省略是"该字段未使用"，`null` 是"该字段有值且值为空"。
对可选对象统一用「省略」表达缺失，避免读者把 `null` 当作第三种状态处理 —— 这一点已写进 v2 §3.7。

### 2.2 两项待确认解释：按文档当前方案定案

| 项 | 负责人裁定 | 文档对应 |
|---|---|---|
| `loongbones.skeleton.v1` 适用范围 | **同意每个 v2 文档都声明为 required**；`project.skeletons` 是 v2 必需键，即使为空，也可以在读取数据之前检查能力声明 | v2 §6 的「若负责人本意是条件式要求，请在复核时指出」已替换为**已裁定**记录 |
| `SkinEntry.id` | **同意保留**。用稳定 ID 保障未知字段在重排、重命名后的归属，比省下约 1.24 MB 的 ID 字符串更重要；索引锚点不符合既定契约 | v2 §5.3 决策记录保留；B0 报告 §3 的第 2 项解释相应关闭 |

### 2.3 DragonBones 对照范围（负责人要求注明）

已在四处加注，措辞一致：

| 处 | 加注内容 |
|---|---|
| v2 schema 文档头 | 「证据范围声明」：字段设计依据全部来自 Spine 3.2.01 语料，尚未完成 DragonBones 数据对照；结论只代表 Spine 路径，不得表述为跨格式通用结论；DragonBones 须独立立项 |
| `skeleton-domain-rfc.md` 头 | 同上 |
| `canonical-visual-data-types.md` §7.0 | 四张功能对照表前加「证据范围」块：证据行全部来自 Spine；**不阻塞 Spine 首片（B1）**，只是划定适用范围；DragonBones 立项后应回头复核「Loongbones 契约」与「类型分层」两行 |
| `canonical-visual-data-types.md` 头 | 一行指向 §7.0 |

---

## 3. B0 验收标准自查

| B0 验收原话 | 结果 |
|---|---|
| Schema 示例可由读者独立实现 | ✅ §2 示例为完整合法文档；字段含义/必需性/默认值/枚举在 §3 全表覆盖，未出现"见别处"的必需字段 |
| 所有数组对象的 ID 与未知字段宿主确定 | ✅ `skeletons`/`bones`/`slots`/`skins`/`entries` 全部有 ID；`atlasIds`/`boneOrder`/`drawOrder` 是 ID 引用数组（其元素本身不是宿主对象）；无 ID 的嵌套容器（`sourceRef`/`importedBy`/`color`/`attachment`）锚点规则见 §5.2。补项修订后这些容器全部**可选**，锚点规则只在容器出现时生效；原生创建（无 `sourceRef`）不影响锚点集合 |
| 资源 URI/复制事务没有待定语义 | ✅ §7：路径格式、命名依据、写盘时机、回滚、崩溃窗口、权限边界、Format 层形状校验规则 |
| 诊断等级没有待定语义 | ✅ §9 表逐类列出 error/warning 与后果 |
| 格式迁移的保存时机没有待定语义 | ✅ §8：纯函数、幂等、先入内存、只有用户保存才写 v2 |

---

## 3. 已定案的两项解释（2026-10-20 负责人裁定）

1. **`loongbones.skeleton.v1` 的适用范围**：**每个 v2 文档都声明为 required**；
   `project.skeletons` 是 v2 必需键，即使为空，也可以在读取数据之前检查能力声明。
   （本稿的操作化读法与裁定一致，v2 §6 已从「待确认」改为「已裁定」。）

2. **`SkinEntry.id`**：**保留**。稳定 ID 保障未知字段在重排、重命名后的归属，
   比省下约 1.24 MB 的 ID 字符串更重要；索引锚点不符合既定契约（D22）。
   （代价核算：语料 skin 条目 47,709 个 × 26 字节 ≈ 1.24 MB，Full 口径；已判定为可接受。）

---

## 4. 本工作包未做的事

- 未写任何产品代码；未改 `format/`、`domain/`、`importers/`、`tests/`、`CMakeLists.txt`。
- 未启动 B1–B6。
- 未重跑探针（本轮无统计数据变化）；`make-fixture.mjs --verify` 复跑通过（5 个 fixture）。
- 未改写已审阅的 `docs/format/native-project-v1-rfc.md`；v2 文档是**新增**文件。
- 未开始 DragonBones 格式研究（按裁定独立立项）。

---

## 5. 交付物清单

| 文件 | 变化 |
|---|---|
| `docs/format/native-project-v2-skeleton.md` | **新建**（v2 schema 冻结稿，11 节）；补项修订 `sourceRef` 为整个对象可选；头部加 DragonBones 证据范围声明；§6 能力适用范围改为已裁定 |
| `docs/skeleton/canonical-visual-data-types.md` | 状态改为 B0 冻结稿；§6 改为「已冻结项 + 仍未冻结」；新增 §7.0 四张功能对照表 + 证据范围声明；头部加证据范围一行 |
| `docs/skeleton/skeleton-domain-rfc.md` | §5.4 补 SkinEntry ID 与附件 `type`；§9.1 补 `SKELETON_SKIN_ENTRY_DUPLICATE`；§12 登记 v2 schema；补项修订 `sourceRef` 可选（§5.1/§8.3/§10.1）；头部加 DragonBones 证据范围声明 |
| `docs/skeleton/plugin-host-rfc.md` | §4.1 补 `IResourceStaging`；新增 §7.5 资源落地；§9 补资源落地与诊断等级验收项 |
| `docs/skeleton/README.md` | 状态与文档索引更新 |
| `Agent_Task/Task4.md` | 状态与 B0 完成说明 |
| `STRUCTURE.md` | 三处状态更新 + 登记 v2 schema 与 B0 报告 |
| `docs/skeleton/B0_REPORT.md` | **新建**（本文件；含退回补项记录） |
