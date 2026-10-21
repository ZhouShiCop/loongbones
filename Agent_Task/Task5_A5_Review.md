# Task 5 A5 复核记录（合成 fixture）

- 复核日期：2026-10-22
- 复核对象：`docs/skeleton/fixtures/` 的 6 个新 fixture（06–11）+ `make-fixture.mjs` + fixtures README
- 方法：① 复跑生成器 `--verify` 看是否可复现；② 用**独立探针** `type-byte-census.mjs`
  （按 `binary_reader.cpp` 已验证布局走字节，不经旧 Reader）重解析全部 fixture；
  ③ 逐项解码附件载荷与约束，与 README §2 的 golden 期望**逐值对拍**；
  ④ 与 A1 语料事实、D29–D36 裁定、Task5 §A5 规格交叉核对；⑤ 纪律核查。
- **结论：通过** —— golden 值全部对得上、TBD-F 硬要求已落实。
  首轮发现 1 项事实错误（已就地更正）与 4 项覆盖缺口；**§8 已按 2026-10-22 负责人裁定
  补齐 5 个 fixture（12–16）**，P1–P6 全部处置完毕（当前共 **16 个** fixture）。

---

## 1. 可复现性 ✅

`node tools/skeleton-probe/make-fixture.mjs --verify` 实测输出与 fixtures README 引用块一致：

| fixture | 生成器回读 | 与 README 一致 |
|---|---|---|
| 01 / 02 / 06 / 07 / 09 / 10 / 11 | `ok`，bones/slots/skins 与 README 期望一致 | ✅ |
| 03-bad-parent-index | `ok warnings=1`（旧 Reader 只 warn） | ✅ README 已注明"产品读取器必须拒绝" |
| 04-truncated | `FAIL — 读取 float 越界` | ✅ |
| 05-non-finite-float | `ok warnings=0`（旧 Reader 不校验有限性） | ✅ README 已注明语义差别 |
| 08-linked-mesh | `ok warnings=2`（旧 Reader 找不到父） | ✅ 与 F1 的回退分支现象同源 |

## 2. 独立字节走查 ✅

`type-byte-census.mjs docs/skeleton/fixtures`：16 个单位，14 个走通，
`04-truncated` → `TRUNCATED`（偏移 139 / 文件 142）、`05-non-finite-float` → `NON_FINITE_FLOAT`
（浮点起始偏移 83，README 记的正是 83）按预期失败。

类型字节计数（与 README 清单逐项吻合）：

| 类型 | 数量 | 来自 |
|---|---:|---|
| 0 region | 4 | 01(1) + 02(3) |
| 2 mesh | 3 | 06(1) + 08 的父(1) + 09(1) |
| 3 weightedMesh | 2 | 07(1) + 10(1) |
| **4 linkedMesh** | **2** | 08 的两条子附件 |
| 5 weightedLinkedMesh | 0 | — |
| 1 boundingbox | 0 | D32 占位，无样本 |

## 3. golden 值逐值对拍 ✅

| fixture | README §2 期望 | 字节实测 | 判定 |
|---|---|---|---|
| 06 | `uvs` 6 个浮点、`triangles=[0,1,2]`、`vertices=[0,0,32,0,16,32]`、`hull=3`、`edges=[0,1,1,2,2,0]`、`w=h=32` | `uvs=[0,0,1,0,0.5,1]`、`triangles=[0,1,2]`、`vertices=[0,0,32,0,16,32]`、`hull=3`、`edges=[0,1,1,2,2,0]`、`w=32 h=32` | ✅ |
| 07 | 3 顶点；顶点 0 = `[(bone-a,0,0,1.2),(root,10,0,-0.2)]`，和 = 1.0（D34 负权重合法） | v0 `bc=2 (bone=1 x=0 y=0 w=1.2) (bone=0 x=10 y=0 w=-0.2)`；v1 `w=1`；v2 `0.5+0.5` | ✅ |
| 08 | 同 slot 3 附件：`body_mesh` + 两条子附件（`inheritDeform` true/false），`parentKey=(slot-body, body_mesh)`，子附件不复制顶点 | slot 名 `slot-body`；3 附件；两条 `parent=body_mesh`、`inheritDeform=1/0`；子附件无顶点区 | ✅ |
| 09 | 3 顶点但三角索引含 `9` | `triangles=[0,1,9]`，`vertexCount=3` | ✅ |
| 10 | `boneIndex = 7`（2 根骨骼） | v0 `bc=1 (bone=7 …)`，`bones=2` | ✅ |
| 11 | IK `mix=1` ⇒ `enabled=true`（D35）、`bendPositive=true`；transform 10 个 float 原值透传（D36） | `ik0 mix=1 bendPositive=1`；`tr0 [90,0,0,0,0,0,1,0.5,1,0]` | ✅ |
| 03 | 第 3 根骨骼 `parentIndex=9`（3 根骨骼） | `bone2 parent=9`，`bones=3` | ✅ |
| 06 / 07 / 08 | 附件 `path` 为 null ⇒ 回退用附件名查图集 region | 06 `key=mesh-0` ↔ 图集 region `mesh-0`；07 `wmesh-0` ↔ `wmesh-0`；08 `body_mesh` ↔ `body_mesh` | ✅（region 查找不会未命中） |

## 4. TBD-F 硬要求已落实 ✅

Task5 §A5 因 TBD-F 结论追加的要求是「linked fixture **必须显式覆盖类型 4**（父为非加权 mesh）」。
实测 08 的两条子附件**类型字节 = 4**，父 `body_mesh` 类型字节 = 2（非加权 mesh），
`inheritDeform` true / false 各一 ⇒ 硬要求满足。语料 2 条真实样本（类型 5、父为类型 3）
覆盖的是另一条路径，两条路径现在都有样本。

## 5. 问题清单

| # | 严重度 | 问题 | 处置 |
|---|---|---|---|
| **P1** | **事实错误** | fixtures README §1 称 06 的 `width/height`（`nonessential` 形态）「**与语料 20,250 条 mesh 一致**」。实测语料 **nonessential=1 只有 4 / 1113 单位（0.36%）**，`edges` 出现率 **67 / 20,250 = 0.33%**（A1 §4.3）⇒ 该形态是**极少数**，占 99%+ 的 `nonessential = 0`（无 `edges` / `width` / `height`）形态**没有 mesh fixture** | README 该行已改写为实测口径；**已补 `16-minimal-mesh-plain`**（`nonessential = 0`，2026-10-22，见 §8.1） |
| **P2** | **规格未满足** | Task5 §A5 要求「**非法权重**/索引（畸形引用）」。现 09（三角索引越界）、10（`boneIndex` 越界）覆盖的是**索引/引用**；**权重和非法**（`sum = 0` / NaN / 越界 → **D30 要求拒绝**、D34 明确"导入期只校验权重和：有限且非 0"）**没有任何 fixture** | **已补（2026-10-22）**：新增 `12-mesh-bad-weight-sum-zero`（和 = 0）与 `13-mesh-bad-weight-sum-negative`（和 = −0.7）。负责人裁定「和为 0 即可，小于零也没有意义」⇒ 判据收紧为**和必须有限且 > 0**，记为 **D37** |
| **P3** | 覆盖缺口（中） | 08 的 `parentSkin` 写**显式 `"default"`**；而语料 2 条真实样本是 `parentSkin = null`（＝同皮肤）。二者是**不同的解析路径**（`findSkin("default")` vs 当前皮肤），fixture 只覆盖了语料里不存在的那条 | **已补（2026-10-22）**：新增 `14-linked-mesh-implicit-skin`（类型 5 + `parentSkin = null` + 父为加权 mesh），与语料 2 例逐字段对齐；08 保留为类型 4 一侧。记为 **D38** |
| **P4** | 覆盖缺口（低） | **D35** `enabled = (mix != 0)`：11 只有 `mix = 1` ⇒ `true`；语料有 **302 条 `mix = 0`** ⇒ `false` 分支无 fixture | **已补（2026-10-22）**：新增 `15-ik-mix-zero`（`mix = 0` ⇒ `enabled = false`，`bendPositive = false`） |
| **P5** | 工具缺陷（非 A5） | `type-byte-census.mjs` 的父引用解析只处理 `parentSkin = null`，遇显式皮肤名会误报「父附件未找到」（跑 fixtures 时命中 08） | **已修**：`skinMatches()` 同时匹配同名皮肤与 default 皮肤；修复后 08 解析出父类型 2，语料结果**无回归**（region/mesh/weightedMesh/linked 仍逐位一致） |
| **P6** | 命名歧义（低） | 模型提案 §12 的 fixture 逻辑名（`v3-mesh-min` / `v3-mesh-weighted` / `v3-mesh-linked` / `v3-mesh-invalid` / `v3-constraints`）与落盘命名（06/07/08/09+10/11）对不上，且 RFC 里 `v3-mesh-invalid` 是一个、实际拆成两个 | 已在 fixtures README §1 末加**命名对应表** |

## 6. 纪律核查 ✅

- 不含游戏资产：全部由 `make-fixture.mjs` 按 3.2.01 布局独立写出；PNG 为脚本生成的纯色图。
- 不含绝对路径 / 盘符 / 用户名（生成器 stdout 里的目录是运行时输出，不入库）。
- `fixtures.json` 已同步 **16** 项；`STRUCTURE.md` §2.4 已改为「16 个：5 个首批 + 11 个第二批」。
- fixtures README §4 明确列出语料 0 例、只能由 fixture 覆盖的能力，并带 **D13 提醒**
  （合成样本 ≠ 兼容声明；07 的负权重语义来自 D34、08 / 14 的父引用解析尚未被任何实现验证、
  11 / 15 的 mix 按 D35 只记录不解释、12 / 13 的拒绝阈值来自 D37）。

## 7. 停点

A5 **已产出，P1–P6 全部处置完毕**（P1 / P5 / P6 在前一轮就地更正，P2–P4 在本轮补齐），
判定升级为**通过**。是否放行进阶段 B，待负责人对阶段 A 整体（A1–A5）作一次验收。

---

## 8. 本轮补齐记录（2026-10-22，负责人裁定后执行）

负责人裁定（原文）：

> 「非法权重和为 0 即可，小于零也没有意义 /
> 最好对齐，不过不对齐也没问题，解析器需要对齐，我们用的（原生）格式没关系 / D35 需要处理」

### 8.1 新增 5 个 fixture（12–16）

| fixture | 对应裁定 / 缺口 | golden（独立探针直读字节验证） |
|---|---|---|
| `12-mesh-bad-weight-sum-zero` | P2 | 类型 3；`v0 = [(bone=1,0,0,**0**), (bone=0,10,0,**0**)]` ⇒ **和 = 0.000000**；v1/v2 和 = 1 |
| `13-mesh-bad-weight-sum-negative` | P2 | 类型 3；`v0 = [(bone=1,0,0,**−0.5**), (bone=0,10,0,**−0.2**)]` ⇒ **和 = −0.700000**（float32 精确值 −0.20000000298023224） |
| `14-linked-mesh-implicit-skin` | P3 | 父 `body_wmesh` 类型 **3**；两条子附件类型 **5**、`parentSkin = null`、`inheritDeform` 1 / 0 |
| `15-ik-mix-zero` | P4 | `ik[0] name=leg-ik-off bones=[1] target=0 **mix=0** bendPositiveByte=255`（＝false） |
| `16-minimal-mesh-plain` | P1 遗留缺口 | `nonessential = 0`：无 `edges` / `width` / `height`、骨骼末尾无 4 字节颜色；`uvs=[0,0,1,0,0.5,1]`、`triangles=[0,1,2]`、`vertices=[0,0,32,0,16,32]`、`hull=3` |

> **16 是"对齐语料"这条裁定的另一半**：语料 **1109 / 1113 单位**是 `nonessential = 0`，
> 而 06–15 全部走 `nonessential = 1`（仅 4 个单位的形态）。它与 14 同属
> 「fixture 尽量对齐语料主流形态」—— 负责人：*「最好对齐，不过不对齐也没问题，
> 解析器需要对齐，我们用的（原生）格式没关系」*。

- 12 / 13 只钉「顶点 0 的和」：拒绝是**候选级**的，先命中的非法顶点即终止解析，
  因此一个 fixture 里放多个非法顶点没有意义（后面的不会被测到）。
- 14 与 08 并列后，linked 的两条路径**各有样本**：类型 4（语料 0 例）/ 类型 5（语料 2 例）。

### 8.2 规则收紧：权重和判据（**D37**）

原表述（D30 / D34 / RFC §11）：「权重和有限且**非 0**，不含负值」⇒ 隐含 `sum < 0` 不拒绝。
改为：**和必须有限且 > 0**；`sum = 0`、`sum < 0`、NaN / ±Inf 均 `MESH_WEIGHT_SUM_INVALID` 拒绝。

- 判据作用在**权重和**，**不是**单条绑定的符号：单条负权重只要和仍 > 0 就合法
  （D34 的语料 86 例与 fixture 07 的 `1.2 / −0.2` 不受影响）。
- 同步改动：`DECISIONS.md` **D34 加注 + 新增 D37 / D38**、`mesh-and-constraint-rfc.md`
  §11 诊断行与 §12 golden 计划表。

### 8.3 生成器缺陷修复：`parentSkin` 显式 null 被吞

`writeLinkedMesh` 原写 `w.str(a.parentSkin ?? 'default')` —— `??` 会把**显式 `null`** 也
判为空值并回退成 `'default'`，导致「父在同皮肤内」这条语料真实路径**永远写不出来**。
改为 `hasOwnProperty` 判空后：不传字段才回退 `'default'`，显式 `null` 原样写出。
修复**不影响 08**（它显式传 `'default'`），已复跑全目录确认。

### 8.4 语料侧取证：D37 的拒绝路径**没有真实样本**

新增 `tools/skeleton-probe/weight-sum-census.mjs`（按 `binary_reader.cpp` 布局独立走字节，
不复用旧 Reader），全语料 **1113 / 1113** 单位、加权顶点 **312,083**：

| 项 | 数量 |
|---|---:|
| `sum = 0` | **0** |
| `sum < 0` | **0** |
| 非有限（NaN / ±Inf） | **0** |
| `\|sum − 1\| > 1e-4` | **0** |
| 单条负权重绑定（D34 合法） | **86** |
| 区间 | min **0.9999299766495824** / max **1.0000100044999272** |

⇒ **D37 的拒绝路径只能由 12 / 13 两个合成 fixture 覆盖**，阶段 B 不得表述为「语料已验证」。
顺带**独立复验**了 A1 的两个修正值：加权顶点 **312,083**（与 A1 更正后一致，原 595,995 确为权重条目数）、
单条负权重 **86**（与 A1 一致）—— 两条互证，A1 的更正成立。

### 8.5 复核动作

- `make-fixture.mjs --verify`：16 个全部写出，回读无新增异常（04 仍 FAIL = 期望）。
- `type-byte-census.mjs` 对 `docs/skeleton/fixtures/` 独立走字节：类型 4 **2** 条（08）、
  类型 5 **2** 条（14），父类型分别 `mesh(2)` / `weightedMesh(3)` —— 与生成器意图逐条一致。
- 语料侧复跑**无回归**：region / mesh / weightedMesh / linked 计数仍与 A1 逐位一致。
